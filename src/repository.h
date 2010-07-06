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

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <QHash>
#include <QProcess>
#include <QVector>

#include "ruleparser.h"

class Repository
{
public:
    class Transaction
    {
        Q_DISABLE_COPY(Transaction)
        friend class Repository;

        Repository *repository;
        QByteArray branch;
        QByteArray svnprefix;
        QByteArray author;
        QByteArray log;
        uint datetime;
        int revnum;

	QVector<int> merges;

        QStringList deletedFiles;
        QByteArray modifiedFiles;

        inline Transaction() {}
    public:
        ~Transaction();
        void commit();

        void setAuthor(const QByteArray &author);
        void setDateTime(uint dt);
        void setLog(const QByteArray &log);

	void noteCopyFromBranch (const QString &prevbranch, int revFrom);

        void deleteFile(const QString &path);
        QIODevice *addFile(const QString &path, int mode, qint64 length);
    };
    Repository(const Rules::Repository &rule);
    ~Repository();

    void reloadBranches();
    void createBranch(const QString &branch, int revnum,
                      const QString &branchFrom, int revFrom);
    void deleteBranch(const QString &branch, int revnum);
    Transaction *newTransaction(const QString &branch, const QString &svnprefix, int revnum);

    void createAnnotatedTag(const QString &name, const QString &svnprefix, int revnum,
                            const QByteArray &author, uint dt,
                            const QByteArray &log);
    void finalizeTags();

private:
    struct Branch
    {
        int created;
        QVector<int> commits;
	QVector<int> marks;
    };
    struct AnnotatedTag
    {
        QString supportingRef;
        QByteArray svnprefix;
        QByteArray author;
        QByteArray log;
        uint dt;
        int revnum;
    };

    QHash<QString, Branch> branches;
    QHash<QString, AnnotatedTag> annotatedTags;
    QString name;
    QProcess fastImport;
    int commitCount;
    int outstandingTransactions;
    int lastmark;
    bool processHasStarted;

    void startFastImport();
    void closeFastImport();

    friend class ProcessCache;
    Q_DISABLE_COPY(Repository)
};

#endif
