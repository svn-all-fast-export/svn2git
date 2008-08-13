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

#include "repository.h"
#include <QTextStream>
#include <QDebug>

Repository::Repository(const Rules::Repository &rule)
    : name(rule.name), processHasStarted(false)
{
    foreach (Rules::Repository::Branch branchRule, rule.branches) {
        Branch branch;
        branch.created = 0;     // not created

        branches.insert(branchRule.name, branch);
    }

    // create the default branch
    branches["master"].created = 1;

    fastImport.setWorkingDirectory(name);
}

Repository::~Repository()
{
    if (fastImport.state() != QProcess::NotRunning) {
        fastImport.closeWriteChannel();
        fastImport.waitForFinished();
    }
}

void Repository::reloadBranches()
{
    QHash<QString, Branch>::Iterator it = branches.begin(),
                                    end = branches.end();
    for ( ; it != end; ++it) {
        QString branchRef = it.key();
        if (!branchRef.startsWith("refs/"))
            branchRef.prepend("refs/heads/");

        bool branchExists;
        // does this branch already exist?
        QProcess revParse;
        revParse.setWorkingDirectory(name);
        revParse.start("git-rev-parse", QStringList() << "--verify" << branchRef);
        revParse.waitForFinished();

        if (revParse.exitCode() == 0)
            branchExists = true;
        else
            branchExists = false;

        if (branchExists) {
            startFastImport();
            fastImport.write("reset " + branchRef.toUtf8() +
                             "\nfrom " + branchRef.toUtf8() + "^0\n\n");
            it->created = 1;
        }
    }
}

void Repository::createBranch(const QString &branch, int revnum,
                              const QString &branchFrom, int)
{
    if (!branches.contains(branch)) {
        qWarning() << branch << "is not a known branch in repository" << name << endl
                   << "Going to create it automatically";
    }

    startFastImport();
    QByteArray branchRef = branch.toUtf8();
    if (!branchRef.startsWith("refs/"))
        branchRef.prepend("refs/heads/");

    Branch &br = branches[branch];
    if (br.created && br.created != revnum) {
        QByteArray backupBranch = branchRef + '_' + QByteArray::number(revnum);
        qWarning() << branch << "already exists; backing up to" << backupBranch;

        fastImport.write("reset " + backupBranch + "\nfrom " + branchRef + "\n\n");
    }

    // now create the branch
    br.created = revnum;
    QByteArray branchFromRef = branchFrom.toUtf8();
    if (!branchFromRef.startsWith("refs/"))
        branchFromRef.prepend("refs/heads/");

    if (!branches.contains(branchFrom) || !branches.value(branchFrom).created) {
        qCritical() << branch << "in repository" << name
                    << "is branching from branch" << branchFrom
                    << "but the latter doesn't exist. Can't continue.";
        exit(1);
    }

    fastImport.write("reset " + branchRef + "\nfrom " + branchFromRef + "\n\n");
}

Repository::Transaction *Repository::newTransaction(const QString &branch, const QString &svnprefix,
                                                    int revnum)
{
    if (!branches.contains(branch)) {
        qCritical() << branch << "is not a known branch in repository" << name;
        return 0;
    }

    Transaction *txn = new Transaction;
    txn->repository = this;
    txn->branch = branch.toUtf8();
    txn->svnprefix = svnprefix.toUtf8();
    txn->datetime = 0;
    txn->revnum = revnum;
    txn->lastmark = revnum;

    startFastImport();
    if ((++commitCount % 10000) == 0)
        // write everything to disk every 10000 commits
        fastImport.write("checkpoint\n");
    return txn;
}

void Repository::startFastImport()
{
    if (fastImport.state() == QProcess::NotRunning) {
        if (processHasStarted)
            qFatal("git-fast-import has been started once and crashed?");
        processHasStarted = true;

        // start the process
        QString outputFile = name;
        outputFile.replace('/', '_');
        outputFile.prepend("log-");
        fastImport.setStandardOutputFile(outputFile, QIODevice::Append);
        fastImport.setProcessChannelMode(QProcess::MergedChannels);

#ifndef DRY_RUN
        fastImport.start("git-fast-import", QStringList());
#else
        fastImport.start("/bin/cat", QStringList());
#endif
    }
}

Repository::Transaction::~Transaction()
{
}

void Repository::Transaction::setAuthor(const QByteArray &a)
{
    author = a;
}

void Repository::Transaction::setDateTime(uint dt)
{
    datetime = dt;
}

void Repository::Transaction::setLog(const QByteArray &l)
{
    log = l;
}

void Repository::Transaction::deleteFile(const QString &path)
{
    deletedFiles.append(path);
}

QIODevice *Repository::Transaction::addFile(const QString &path, int mode, qint64 length)
{
    FileProperties fp;
    fp.mode = mode;
    fp.mark = ++lastmark;

#ifndef DRY_RUN
    repository->fastImport.write("blob\nmark :");
    repository->fastImport.write(QByteArray::number(fp.mark));
    repository->fastImport.write("\ndata ");
    repository->fastImport.write(QByteArray::number(length));
    repository->fastImport.write("\n", 1);
#endif

    modifiedFiles.insert(path, fp);
    return &repository->fastImport;
}

void Repository::Transaction::commit()
{
    // create the commit message
    QByteArray message = log;
    if (!message.endsWith('\n'))
        message += '\n';
    message += "\nsvn path=" + svnprefix + "; revision=" + QByteArray::number(revnum) + "\n";

    {
        QByteArray branchRef = branch;
        if (!branchRef.startsWith("refs/"))
            branchRef.prepend("refs/heads/");

        QTextStream s(&repository->fastImport);
        s << "commit " << branchRef << endl;
        s << "mark :" << revnum << endl;
        s << "committer " << QString::fromUtf8(author) << ' ' << datetime << " -0000" << endl;

        Branch &br = repository->branches[branch];
        if (!br.created) {
            qWarning() << "Branch" << branch << "doesn't exist at revision"
                       << revnum << "-- did you resume from the wrong revision?";
            br.created = revnum;
        }

        s << "data " << message.length() << endl;
    }

    repository->fastImport.write(message);
    repository->fastImport.putChar('\n');

    // write the file deletions
    if (deletedFiles.contains(""))
        repository->fastImport.write("deleteall\n");
    else
        foreach (QString df, deletedFiles)
            repository->fastImport.write("D " + df.toUtf8() + "\n");

    // write the file modifications
    QHash<QString, FileProperties>::ConstIterator it = modifiedFiles.constBegin();
    for ( ; it != modifiedFiles.constEnd(); ++it) {
        repository->fastImport.write("M ", 2);
        repository->fastImport.write(QByteArray::number(it->mode, 8));
        repository->fastImport.write(" :", 2);
        repository->fastImport.write(QByteArray::number(it->mark));
        repository->fastImport.write(" ", 1);
        repository->fastImport.write(it.key().toUtf8());
        repository->fastImport.write("\n", 1);
    }

    repository->fastImport.write("\n");

    while (repository->fastImport.bytesToWrite())
        if (!repository->fastImport.waitForBytesWritten(-1))
            qFatal("Failed to write to process: %s", qPrintable(repository->fastImport.errorString()));
}
