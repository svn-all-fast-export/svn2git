/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
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
public:
    inline AprAutoPool(apr_pool_t *parent = NULL)
        { pool = svn_pool_create(parent); }
    inline ~AprAutoPool()
        { svn_pool_destroy(pool); }

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
    openRepository(pathToRepository);

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
    SVN_ERR(svn_repos_open(&repos, QFile::encodeName(pathToRepository), global_pool));
    fs = svn_repos_fs(repos);

    return EXIT_SUCCESS;
}

static int pathMode(svn_fs_root_t *fs_root, const char *pathname, apr_pool_t *pool)
{
    svn_string_t *propvalue;
    SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:executable", pool));
    int mode = 0100644;
    if (propvalue)
        mode = 0100755;

    // maybe it's a symlink?
    SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, pathname, "svn:special", pool));
    if (strcmp(propvalue->data, "symlink") == 0)
        mode = 0120000;

    return mode;
}

svn_error_t *QIODevice_write(void *baton, const char *data, apr_size_t *len)
{
    QIODevice *device = reinterpret_cast<QIODevice *>(baton);
    device->write(data, *len);
    return SVN_NO_ERROR;
}

static svn_stream_t *streamForDevice(QIODevice *device, apr_pool_t *pool)
{
    svn_stream_t *stream = svn_stream_create(device, pool);
    svn_stream_set_write(stream, QIODevice_write);

    return stream;
}

static int dumpBlob(Repository::Transaction *txn, svn_fs_root_t *fs_root,
                    const char *pathname, apr_pool_t *pool)
{
    // what type is it?
    int mode = pathMode(fs_root, pathname, pool);

    svn_stream_t *in_stream, *out_stream;
    svn_filesize_t stream_length;

    SVN_ERR(svn_fs_file_length(&stream_length, fs_root, pathname, pool));
    QIODevice *io = txn->addFile(pathname, mode, stream_length);

    // open the file
    SVN_ERR(svn_fs_file_contents(&in_stream, fs_root, pathname, pool));

    // open a generic svn_stream_t for the QIODevice
    out_stream = streamForDevice(io, pool);
    SVN_ERR(svn_stream_copy(in_stream, out_stream, pool));

    return EXIT_SUCCESS;
}

time_t get_epoch(char *svn_date)
{
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    QByteArray date(svn_date, strlen(svn_date) - 8);
    strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
}

int SvnPrivate::exportRevision(int revnum)
{
    AprAutoPool pool(global_pool);

    // open this revision:
    svn_fs_root_t *fs_root;
    SVN_ERR(svn_fs_revision_root(&fs_root, fs, revnum, pool));
    qDebug() << "Exporting revision" << revnum;

    // find out what was changed in this revision:
    QHash<QString, Repository::Transaction *> transactions;
    apr_hash_t *changes;
    SVN_ERR(svn_fs_paths_changed(&changes, fs_root, pool));
    AprAutoPool revpool(pool);
    for (apr_hash_index_t *i = apr_hash_first(pool, changes); i; i = apr_hash_next(i)) {
        svn_pool_clear(revpool);

        const void *vkey;
        void *value;
        apr_hash_this(i, &vkey, NULL, &value);
        const char *key = reinterpret_cast<const char *>(vkey);

        // is this a directory?
        svn_boolean_t is_dir;
        SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, key, revpool));
        if (is_dir)
            continue;           // Git doesn't handle directories, so we don't either

        QString current = QString::fromUtf8(key);

        // find the first rule that matches this pathname
        bool foundMatch = false;
        foreach (Rules::Match rule, matchRules)
            if (rule.rx.exactMatch(current)) {
                foundMatch = true;
                QString repository = current;
                QString branch = current;
                QString path = current;

                // do the replacement
                repository.replace(rule.rx, rule.repository);
                branch.replace(rule.rx, rule.branch);
                path.replace(rule.rx, rule.path);

                qDebug() << "..." << current << "->"
                         << repository << branch << path;

                Repository::Transaction *txn = transactions.value(repository, 0);
                if (!txn) {
                    Repository *repo = repositories.value(repository, 0);
                    if (!repo) {
                        qCritical() << "Rule" << rule.rx.pattern()
                                    << "references unknown repository" << repository;
                        return EXIT_FAILURE;
                    }

                    QString svnprefix = current;
                    if (current.endsWith(path))
                        current.chop(path.length());

                    txn = repo->newTransaction(branch, svnprefix, revnum);
                    if (!txn)
                        return EXIT_FAILURE;

                    transactions.insert(repository, txn);
                }

                svn_fs_path_change_t *change = reinterpret_cast<svn_fs_path_change_t *>(value);
                if (change->change_kind == svn_fs_path_change_delete)
                    txn->deleteFile(path);
                else
                    dumpBlob(txn, fs_root, key, revpool);

                break;
            }

        if (!foundMatch) {
            qCritical() << current << "did not match any rules; cannot continue";
            return EXIT_FAILURE;
        }
    }
    svn_pool_clear(revpool);

    if (transactions.isEmpty())
        return true;            // no changes?

    // now create the commit
    apr_hash_t *revprops;
    SVN_ERR(svn_fs_revision_proplist(&revprops, fs, revnum, pool));
    svn_string_t *svnauthor = (svn_string_t*)apr_hash_get(revprops, "svn:author", APR_HASH_KEY_STRING);
    svn_string_t *svndate = (svn_string_t*)apr_hash_get(revprops, "svn:date", APR_HASH_KEY_STRING);
    svn_string_t *svnlog = (svn_string_t*)apr_hash_get(revprops, "svn:log", APR_HASH_KEY_STRING);

    QByteArray log = (char *)svnlog->data;
    QByteArray authorident = identities.value((char *)svnauthor->data);
    time_t epoch = get_epoch((char*)svndate->data);
    if (authorident.isEmpty()) {
        if (!svnauthor || svn_string_isempty(svnauthor))
            authorident = "nobody <nobody@localhost>";
        else
            authorident = svnauthor->data + QByteArray(" <") +
                          svnauthor->data + QByteArray("@localhost>");
    }

    foreach (Repository::Transaction *txn, transactions) {
        txn->setAuthor(authorident);
        txn->setDateTime(epoch);
        txn->setLog(log);

        txn->commit();
        delete txn;
    }

    return EXIT_SUCCESS;
}
