/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Based on svn-fast-export by Chris Lee <clee@kde.org>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 * URL: git://repo.or.cz/fast-import.git http://repo.or.cz/w/fast-export.git
 */

#define _XOPEN_SOURCE
#define _LARGEFILE_SUPPORT
#define _LARGEFILE64_SUPPORT

#include "svn.h"
#include "CommandLineParser.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <apr_lib.h>
#include <apr_getopt.h>
#include <apr_general.h>

#include <svn_fs.h>
#include <svn_pools.h>
#include <svn_repos.h>
#include <svn_types.h>

#include <QFile>
#include <QDebug>

#include "repository.h"

#undef SVN_ERR
#define SVN_ERR(expr) SVN_INT_ERR(expr)

typedef QList<Rules::Match> MatchRuleList;
typedef QHash<QString, Repository *> RepositoryHash;
typedef QHash<QByteArray, QByteArray> IdentityHash;

class AprAutoPool
{
    apr_pool_t *pool;
    AprAutoPool(const AprAutoPool &);
    AprAutoPool &operator=(const AprAutoPool &);
public:
    inline AprAutoPool(apr_pool_t *parent = NULL)
        { pool = svn_pool_create(parent); }
    inline ~AprAutoPool()
        { svn_pool_destroy(pool); }

    inline void clear() { svn_pool_clear(pool); }
    inline apr_pool_t *data() const { return pool; }
    inline operator apr_pool_t *() const { return pool; }
};

class SvnPrivate
{
public:
    MatchRuleList matchRules;
    RepositoryHash repositories;
    IdentityHash identities;

    SvnPrivate(const QString &pathToRepository);
    ~SvnPrivate();
    int youngestRevision();
    int exportRevision(int revnum);

    int openRepository(const QString &pathToRepository);

private:
    AprAutoPool global_pool;
    svn_fs_t *fs;
    svn_revnum_t youngest_rev;
};

void Svn::initialize()
{
    // initialize APR or exit
    if (apr_initialize() != APR_SUCCESS) {
        fprintf(stderr, "You lose at apr_initialize().\n");
        exit(1);
    }

    // static destructor
    static struct Destructor { ~Destructor() { apr_terminate(); } } destructor;
}

Svn::Svn(const QString &pathToRepository)
    : d(new SvnPrivate(pathToRepository))
{
}

Svn::~Svn()
{
    delete d;
}

void Svn::setMatchRules(const MatchRuleList &matchRules)
{
    d->matchRules = matchRules;
}

void Svn::setRepositories(const RepositoryHash &repositories)
{
    d->repositories = repositories;
}

void Svn::setIdentityMap(const IdentityHash &identityMap)
{
    d->identities = identityMap;
}

int Svn::youngestRevision()
{
    return d->youngestRevision();
}

bool Svn::exportRevision(int revnum)
{
    return d->exportRevision(revnum) == EXIT_SUCCESS;
}

SvnPrivate::SvnPrivate(const QString &pathToRepository)
    : global_pool(NULL)
{
    if( openRepository(pathToRepository) != EXIT_SUCCESS) {
        qCritical() << "Failed to open repository";
        exit(1);
    }

    // get the youngest revision
    svn_fs_youngest_rev(&youngest_rev, fs, global_pool);
}

SvnPrivate::~SvnPrivate()
{
    svn_pool_destroy(global_pool);
}

int SvnPrivate::youngestRevision()
{
    return youngest_rev;
}

int SvnPrivate::openRepository(const QString &pathToRepository)
{
    svn_repos_t *repos;
    QString path = pathToRepository;
    while (path.endsWith('/')) // no trailing slash allowed
        path = path.mid(0, path.length()-1);
    SVN_ERR(svn_repos_open(&repos, QFile::encodeName(path), global_pool));
    fs = svn_repos_fs(repos);

    return EXIT_SUCCESS;
}

enum RuleType { AnyRule = 0, NoIgnoreRule = 0x01, NoRecurseRule = 0x02 };

static MatchRuleList::ConstIterator
findMatchRule(const MatchRuleList &matchRules, int revnum, const QString &current,
              int ruleMask = AnyRule)
{
    MatchRuleList::ConstIterator it = matchRules.constBegin(),
                                end = matchRules.constEnd();
    for ( ; it != end; ++it) {
        if (it->minRevision > revnum)
            continue;
        if (it->maxRevision != -1 && it->maxRevision < revnum)
            continue;
        if (it->action == Rules::Match::Ignore && ruleMask & NoIgnoreRule)
            continue;
        if (it->action == Rules::Match::Recurse && ruleMask & NoRecurseRule)
            continue;
        if (it->rx.indexIn(current) == 0) {
            return it;
        }
    }

    // no match
    return end;
}

static void splitPathName(const Rules::Match &rule, const QString &pathName, QString *svnprefix_p,
                          QString *repository_p, QString *branch_p, QString *path_p)
{
    QString svnprefix = pathName;
    svnprefix.truncate(rule.rx.matchedLength());

    if (svnprefix_p) {
        *svnprefix_p = svnprefix;
    }

    if (repository_p) {
        *repository_p = svnprefix;
        repository_p->replace(rule.rx, rule.repository);
    }

    if (branch_p) {
        *branch_p = svnprefix;
        branch_p->replace(rule.rx, rule.branch);
    }

    if (path_p) {
        QString prefix = svnprefix;
        prefix.replace(rule.rx, rule.prefix);
        *path_p = prefix + pathName.mid(svnprefix.length());
    }
}

static int pathMode(svn_fs_root_t *fs_root, const char *pathname, apr_pool_t *pool)
{
    svn_string_t *propvalue;
    SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:executable", pool));
    int mode = 0100644;
    if (propvalue)
        mode = 0100755;

    return mode;
}

svn_error_t *QIODevice_write(void *baton, const char *data, apr_size_t *len)
{
    QIODevice *device = reinterpret_cast<QIODevice *>(baton);
    device->write(data, *len);

    while (device->bytesToWrite() > 32*1024) {
        if (!device->waitForBytesWritten(-1)) {
            qFatal("Failed to write to process: %s", qPrintable(device->errorString()));
            return svn_error_createf(APR_EOF, SVN_NO_ERROR, "Failed to write to process: %s",
                                     qPrintable(device->errorString()));
        }
    }
    return SVN_NO_ERROR;
}

static svn_stream_t *streamForDevice(QIODevice *device, apr_pool_t *pool)
{
    svn_stream_t *stream = svn_stream_create(device, pool);
    svn_stream_set_write(stream, QIODevice_write);

    return stream;
}

static int dumpBlob(Repository::Transaction *txn, svn_fs_root_t *fs_root,
                    const char *pathname, const QString &finalPathName, apr_pool_t *pool)
{
    AprAutoPool dumppool(pool);
    // what type is it?
    int mode = pathMode(fs_root, pathname, dumppool);

    svn_filesize_t stream_length;

    SVN_ERR(svn_fs_file_length(&stream_length, fs_root, pathname, dumppool));

    svn_stream_t *in_stream, *out_stream;
    if (!CommandLineParser::instance()->contains("dry-run")) {
        // open the file
        SVN_ERR(svn_fs_file_contents(&in_stream, fs_root, pathname, dumppool));
    }

    // maybe it's a symlink?
    svn_string_t *propvalue;
    SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:special", dumppool));
    if (propvalue) {
        apr_size_t len = strlen("link ");
        if (!CommandLineParser::instance()->contains("dry-run")) {
            QByteArray buf;
            buf.reserve(len);
            SVN_ERR(svn_stream_read(in_stream, buf.data(), &len));
            if (len != strlen("link ") || strncmp(buf, "link ", len) != 0)
                qFatal("file %s is svn:special but not a symlink", pathname);
        }
        mode = 0120000;
        stream_length -= len;
    }

    QIODevice *io = txn->addFile(finalPathName, mode, stream_length);

    if (!CommandLineParser::instance()->contains("dry-run")) {
        // open a generic svn_stream_t for the QIODevice
        out_stream = streamForDevice(io, dumppool);
        SVN_ERR(svn_stream_copy(in_stream, out_stream, dumppool));
        svn_stream_close(out_stream);
        svn_stream_close(in_stream);

        // print an ending newline
        io->putChar('\n');
    }

    return EXIT_SUCCESS;
}

static int recursiveDumpDir(Repository::Transaction *txn, svn_fs_root_t *fs_root,
                            const QByteArray &pathname, const QString &finalPathName,
                            apr_pool_t *pool)
{
    // get the dir listing
    apr_hash_t *entries;
    SVN_ERR(svn_fs_dir_entries(&entries, fs_root, pathname, pool));
    AprAutoPool dirpool(pool);

    for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i)) {
        dirpool.clear();
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);

        svn_fs_dirent_t *dirent = reinterpret_cast<svn_fs_dirent_t *>(value);
        QByteArray entryName = pathname + '/' + dirent->name;
        QString entryFinalName = finalPathName + QString::fromUtf8(dirent->name);

        if (dirent->kind == svn_node_dir) {
            entryFinalName += '/';
            if (recursiveDumpDir(txn, fs_root, entryName, entryFinalName, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        } else if (dirent->kind == svn_node_file) {
            printf("+");
            fflush(stdout);
            if (dumpBlob(txn, fs_root, entryName, entryFinalName, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

static bool wasDir(svn_fs_t *fs, int revnum, const char *pathname, apr_pool_t *pool)
{
    AprAutoPool subpool(pool);
    svn_fs_root_t *fs_root;
    if (svn_fs_revision_root(&fs_root, fs, revnum, subpool) != SVN_NO_ERROR)
        return false;

    svn_boolean_t is_dir;
    if (svn_fs_is_dir(&is_dir, fs_root, pathname, subpool) != SVN_NO_ERROR)
        return false;

    return is_dir;
}

time_t get_epoch(char *svn_date)
{
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    QByteArray date(svn_date, strlen(svn_date) - 8);
    strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
    return timegm(&tm);
}

class SvnRevision
{
public:
    AprAutoPool pool;
    QHash<QString, Repository::Transaction *> transactions;
    MatchRuleList matchRules;
    RepositoryHash repositories;
    IdentityHash identities;

    svn_fs_t *fs;
    svn_fs_root_t *fs_root;
    int revnum;

    // must call fetchRevProps first:
    QByteArray authorident;
    QByteArray log;
    uint epoch;
    bool ruledebug;

    SvnRevision(int revision, svn_fs_t *f, apr_pool_t *parent_pool)
        : pool(parent_pool), fs(f), fs_root(0), revnum(revision)
    {
        ruledebug = CommandLineParser::instance()->contains( QLatin1String("debug-rules"));
    }

    int open()
    {
        SVN_ERR(svn_fs_revision_root(&fs_root, fs, revnum, pool));
        return EXIT_SUCCESS;
    }

    int prepareTransactions();
    int fetchRevProps();
    int commit();

    int exportEntry(const char *path, const svn_fs_path_change_t *change, apr_hash_t *changes);
    int exportDispatch(const char *path, const svn_fs_path_change_t *change,
                       const char *path_from, svn_revnum_t rev_from,
                       apr_hash_t *changes, const QString &current, const Rules::Match &rule,
                       apr_pool_t *pool);
    int exportInternal(const char *path, const svn_fs_path_change_t *change,
                       const char *path_from, svn_revnum_t rev_from,
                       const QString &current, const Rules::Match &rule);
    int recurse(const char *path, const svn_fs_path_change_t *change,
                const char *path_from, svn_revnum_t rev_from,
                apr_hash_t *changes, apr_pool_t *pool);
};

int SvnPrivate::exportRevision(int revnum)
{
    SvnRevision rev(revnum, fs, global_pool);
    rev.matchRules = matchRules;
    rev.repositories = repositories;
    rev.identities = identities;

    // open this revision:
    printf("Exporting revision %d ", revnum);
    fflush(stdout);

    if (rev.open() == EXIT_FAILURE)
        return EXIT_FAILURE;

    if (rev.prepareTransactions() == EXIT_FAILURE)
        return EXIT_FAILURE;

    if (rev.transactions.isEmpty()) {
        printf(" nothing to do\n");
        return EXIT_SUCCESS;    // no changes?
    }

    if (rev.commit() == EXIT_FAILURE)
        return EXIT_FAILURE;

    printf(" done\n");
    return EXIT_SUCCESS;
}

int SvnRevision::prepareTransactions()
{
    // find out what was changed in this revision:
    apr_hash_t *changes;
    SVN_ERR(svn_fs_paths_changed(&changes, fs_root, pool));
    for (apr_hash_index_t *i = apr_hash_first(pool, changes); i; i = apr_hash_next(i)) {
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);
        const char *key = reinterpret_cast<const char *>(vkey);
        svn_fs_path_change_t *change = reinterpret_cast<svn_fs_path_change_t *>(value);

        if (exportEntry(key, change, changes) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int SvnRevision::fetchRevProps()
{
    apr_hash_t *revprops;
    SVN_ERR(svn_fs_revision_proplist(&revprops, fs, revnum, pool));
    svn_string_t *svnauthor = (svn_string_t*)apr_hash_get(revprops, "svn:author", APR_HASH_KEY_STRING);
    svn_string_t *svndate = (svn_string_t*)apr_hash_get(revprops, "svn:date", APR_HASH_KEY_STRING);
    svn_string_t *svnlog = (svn_string_t*)apr_hash_get(revprops, "svn:log", APR_HASH_KEY_STRING);

    log = (char *)svnlog->data;
    authorident = svnauthor ? identities.value((char *)svnauthor->data) : QByteArray();
    epoch = get_epoch((char*)svndate->data);
    if (authorident.isEmpty()) {
        if (!svnauthor || svn_string_isempty(svnauthor))
            authorident = "nobody <nobody@localhost>";
        else
            authorident = svnauthor->data + QByteArray(" <") +
                          svnauthor->data + QByteArray("@localhost>");
    }
    return EXIT_SUCCESS;
}

int SvnRevision::commit()
{
    // now create the commit
    if (fetchRevProps() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    foreach (Repository::Transaction *txn, transactions) {
        txn->setAuthor(authorident);
        txn->setDateTime(epoch);
        txn->setLog(log);

        txn->commit();
        delete txn;
    }

    return EXIT_SUCCESS;
}

int SvnRevision::exportEntry(const char *key, const svn_fs_path_change_t *change,
                             apr_hash_t *changes)
{
    AprAutoPool revpool(pool.data());
    QString current = QString::fromUtf8(key);

    // was this copied from somewhere?
    svn_revnum_t rev_from;
    const char *path_from;
    SVN_ERR(svn_fs_copied_from(&rev_from, &path_from, fs_root, key, revpool));

    // is this a directory?
    svn_boolean_t is_dir;
    SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, key, revpool));
    if (is_dir) {
        if (change->change_kind == svn_fs_path_change_modify ||
            change->change_kind == svn_fs_path_change_add) {
            if (path_from == NULL) {
                // freshly added directory, or modified properties
                // Git doesn't handle directories, so we don't either
                //qDebug() << "   mkdir ignored:" << key;
                return EXIT_SUCCESS;
            }

            qDebug() << "   " << key << "was copied from" << path_from << "rev" << rev_from;
        } else if (change->change_kind == svn_fs_path_change_replace) {
            if (path_from == NULL)
                qDebug() << "   " << key << "was replaced";
            else
                qDebug() << "   " << key << "was replaced from" << path_from << "rev" << rev_from;
        } else if (change->change_kind == svn_fs_path_change_reset) {
            qCritical() << "   " << key << "was reset, panic!";
            return EXIT_FAILURE;
        } else {
            // if change_kind == delete, it shouldn't come into this arm of the 'is_dir' test
            qCritical() << "   " << key << "has unhandled change kind " << change->change_kind << ", panic!";
            return EXIT_FAILURE;
        }
    } else if (change->change_kind == svn_fs_path_change_delete) {
        is_dir = wasDir(fs, revnum - 1, key, revpool);
    }

    if (is_dir)
        current += '/';

    // find the first rule that matches this pathname
    MatchRuleList::ConstIterator match = findMatchRule(matchRules, revnum, current);
    if (match != matchRules.constEnd()) {
        const Rules::Match &rule = *match;
        return exportDispatch(key, change, path_from, rev_from, changes, current, rule, revpool);
    }

    if (is_dir && path_from != NULL) {
        qDebug() << current << "is a copy-with-history, auto-recursing";
        return recurse(key, change, path_from, rev_from, changes, revpool);
    } else if (is_dir && change->change_kind == svn_fs_path_change_delete) {
        qDebug() << current << "deleted, auto-recursing";
        return recurse(key, change, path_from, rev_from, changes, revpool);
    } else if (wasDir(fs, revnum - 1, key, revpool)) {
        qDebug() << current << "was a directory; ignoring";
    } else if (change->change_kind == svn_fs_path_change_delete) {
        qDebug() << current << "is being deleted but I don't know anything about it; ignoring";
    } else {
        qCritical() << current << "did not match any rules; cannot continue";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int SvnRevision::exportDispatch(const char *key, const svn_fs_path_change_t *change,
                                const char *path_from, svn_revnum_t rev_from,
                                apr_hash_t *changes, const QString &current,
                                const Rules::Match &rule, apr_pool_t *pool)
{
    //if(ruledebug)
    //  qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.lineNumber << "(" << rule.rx.pattern() << ")";
    switch (rule.action) {
    case Rules::Match::Ignore:
        //if(ruledebug)
        //    qDebug() << "  " << "ignoring.";
        return EXIT_SUCCESS;

    case Rules::Match::Recurse:
        if(ruledebug)
            qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.lineNumber << "(" << rule.rx.pattern() << ")" << "  " << "recursing.";
        return recurse(key, change, path_from, rev_from, changes, pool);

    case Rules::Match::Export:
        if(ruledebug)
            qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.lineNumber << "(" << rule.rx.pattern() << ")" << "  " << "exporting.";
        if (exportInternal(key, change, path_from, rev_from, current, rule) == EXIT_SUCCESS)
            return EXIT_SUCCESS;
        if (change->change_kind != svn_fs_path_change_delete) {
            if(ruledebug)
                qDebug() << "rev" << revnum << qPrintable(current) << "matched rule:" << rule.lineNumber << "(" << rule.rx.pattern() << ")" << "  " << "Unable to export non path removal.";
            return EXIT_FAILURE;
        }
        // we know that the default action inside recurse is to recurse further or to ignore,
        // either of which is reasonably safe for deletion
        qWarning() << "deleting unknown path" << current << "; auto-recursing";
        return recurse(key, change, path_from, rev_from, changes, pool);
    }

    // never reached
    return EXIT_FAILURE;
}

int SvnRevision::exportInternal(const char *key, const svn_fs_path_change_t *change,
                                const char *path_from, svn_revnum_t rev_from,
                                const QString &current, const Rules::Match &rule)
{
    QString svnprefix, repository, branch, path;
    splitPathName(rule, current, &svnprefix, &repository, &branch, &path);

    Repository *repo = repositories.value(repository, 0);
    if (!repo) {
        if (change->change_kind != svn_fs_path_change_delete)
            qCritical() << "Rule" << rule
                        << "references unknown repository" << repository;
        return EXIT_FAILURE;
    }

    printf(".");
    fflush(stdout);
//                qDebug() << "   " << qPrintable(current) << "rev" << revnum << "->"
//                         << qPrintable(repository) << qPrintable(branch) << qPrintable(path);

    if (change->change_kind == svn_fs_path_change_delete && current == svnprefix && path.isEmpty()) {
        if(ruledebug)
            qDebug() << "repository" << repository << "branch" << branch << "deleted";
        return repo->deleteBranch(branch, revnum);
    }

    QString previous;
    QString prevsvnprefix, prevrepository, prevbranch, prevpath;

    if (path_from != NULL) {
        previous = QString::fromUtf8(path_from);
        AprAutoPool revpool(pool.data());
        svn_boolean_t is_dir;
        SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, path_from, revpool));
        if (is_dir) {
            previous += '/';
        }
        MatchRuleList::ConstIterator prevmatch =
            findMatchRule(matchRules, rev_from, previous, NoIgnoreRule);
        if (prevmatch != matchRules.constEnd()) {
            splitPathName(*prevmatch, previous, &prevsvnprefix, &prevrepository,
                          &prevbranch, &prevpath);
        } else {
            qWarning() << "SVN reports a \"copy from\" but no matching rules found! Ignoring copy, treating as a modification";
            path_from = NULL;
        }
    }

    // current == svnprefix => we're dealing with the contents of the whole branch here
    if (path_from != NULL && current == svnprefix && path.isEmpty()) {
        if (previous != prevsvnprefix) {
            // source is not the whole of its branch
            qDebug() << qPrintable(current) << "is a partial branch of repository"
                     << qPrintable(prevrepository) << "branch"
                     << qPrintable(prevbranch) << "subdir"
                     << qPrintable(prevpath);
        } else if (prevrepository != repository) {
            qWarning() << qPrintable(current) << "rev" << revnum
                       << "is a cross-repository copy (from repository"
                       << qPrintable(prevrepository) << "branch"
                       << qPrintable(prevbranch) << "path"
                       << qPrintable(prevpath) << "rev" << rev_from << ")";
        } else if (path != prevpath) {
            qDebug() << qPrintable(current)
                     << "is a branch copy which renames base directory of all contents"
                     << qPrintable(prevpath) << "to" << qPrintable(path);
            // FIXME: Handle with fast-import 'file rename' facility
            //        ??? Might need special handling when path == / or prevpath == /
        } else {
            if (prevbranch == branch) {
                // same branch and same repository
                qDebug() << qPrintable(current) << "rev" << revnum
                         << "is reseating branch" << qPrintable(branch)
                         << "to an earlier revision"
                         << qPrintable(previous) << "rev" << rev_from;
            } else {
                // same repository but not same branch
                // this means this is a plain branch
                qDebug() << qPrintable(repository) << ": branch"
                         << qPrintable(branch) << "is branching from"
                         << qPrintable(prevbranch);
            }

            if (repo->createBranch(branch, revnum, prevbranch, rev_from) == EXIT_FAILURE)
                return EXIT_FAILURE;
            if (rule.annotate) {
                // create an annotated tag
                fetchRevProps();
                repo->createAnnotatedTag(branch, svnprefix, revnum, authorident,
                                         epoch, log);
            }
            return EXIT_SUCCESS;
        }
    }
    Repository::Transaction *txn = transactions.value(repository + branch, 0);
    if (!txn) {
        txn = repo->newTransaction(branch, svnprefix, revnum);
        if (!txn)
            return EXIT_FAILURE;

        transactions.insert(repository + branch, txn);
    }

    //
    // If this path was copied from elsewhere, use it to infer _some_
    // merge points.  This heuristic is fairly useful for tracking
    // changes across directory re-organizations and wholesale branch
    // imports.
    //
    if (path_from != NULL && prevrepository == repository) {
        if(ruledebug)
            qDebug() << "copy from branch" << prevbranch << "rev" << rev_from;
        txn->noteCopyFromBranch (prevbranch, rev_from);
    }

    if (change->change_kind == svn_fs_path_change_replace && path_from == NULL) {
        if(ruledebug)
            qDebug() << "replaced with empty path (" << branch << path << ")";
        txn->deleteFile(path);
    }
    if (change->change_kind == svn_fs_path_change_delete) {
        if(ruledebug)
            qDebug() << "delete (" << branch << path << ")";
        txn->deleteFile(path);
    } else if (!current.endsWith('/')) {
        if(ruledebug)
            qDebug() << "add/change file (" << key << "->" << branch << path << ")";
        dumpBlob(txn, fs_root, key, path, pool);
    } else {
        if(ruledebug)
            qDebug() << "add/change dir (" << key << "->" << branch << path << ")";
        txn->deleteFile(path);
        recursiveDumpDir(txn, fs_root, key, path, pool);
    }

    return EXIT_SUCCESS;
}

int SvnRevision::recurse(const char *path, const svn_fs_path_change_t *change,
                         const char *path_from, svn_revnum_t rev_from,
                         apr_hash_t *changes, apr_pool_t *pool)
{
    svn_fs_root_t *fs_root = this->fs_root;
    if (change->change_kind == svn_fs_path_change_delete)
        SVN_ERR(svn_fs_revision_root(&fs_root, fs, revnum - 1, pool));

    // get the dir listing
    apr_hash_t *entries;
    svn_node_kind_t kind;
    SVN_ERR(svn_fs_check_path(&kind, fs_root, path, pool));
    if(kind == svn_node_none) {
        qWarning() << "Trying to recurse using a nonexistant path" << path << ", ignoring";
        return EXIT_SUCCESS;
    }

    SVN_ERR(svn_fs_dir_entries(&entries, fs_root, path, pool));

    AprAutoPool dirpool(pool);
    for (apr_hash_index_t *i = apr_hash_first(pool, entries); i; i = apr_hash_next(i)) {
        dirpool.clear();
        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);

        svn_fs_dirent_t *dirent = reinterpret_cast<svn_fs_dirent_t *>(value);
        if (dirent->kind != svn_node_dir)
            continue;           // not a directory, so can't recurse; skip

        QByteArray entry = path + QByteArray("/") + dirent->name;
        QByteArray entryFrom;
        if (path_from)
            entryFrom = path_from + QByteArray("/") + dirent->name;

        // check if this entry is in the changelist for this revision already
        svn_fs_path_change_t *otherchange =
            (svn_fs_path_change_t*)apr_hash_get(changes, entry.constData(), APR_HASH_KEY_STRING);
        if (otherchange && otherchange->change_kind == svn_fs_path_change_add) {
            qDebug() << entry << "rev" << revnum
                     << "is in the change-list, deferring to that one";
            continue;
        }

        QString current = QString::fromUtf8(entry);
        if (dirent->kind == svn_node_dir)
            current += '/';

        // find the first rule that matches this pathname
        MatchRuleList::ConstIterator match = findMatchRule(matchRules, revnum, current);
        if (match != matchRules.constEnd()) {
            if (exportDispatch(entry, change, entryFrom.isNull() ? 0 : entryFrom.constData(),
                               rev_from, changes, current, *match, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        } else {
            qDebug() << current << "rev" << revnum
                     << "did not match any rules; auto-recursing";
            if (recurse(entry, change, entryFrom.isNull() ? 0 : entryFrom.constData(),
                        rev_from, changes, dirpool) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
