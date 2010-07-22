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
    protected:
	Transaction() {}
    public:
	virtual ~Transaction() {}
        virtual void commit() = 0;

        virtual void setAuthor(const QByteArray &author) = 0;
        virtual void setDateTime(uint dt) = 0;
        virtual void setLog(const QByteArray &log) = 0;

	virtual void noteCopyFromBranch (const QString &prevbranch, int revFrom) = 0;

        virtual void deleteFile(const QString &path) = 0;
        virtual QIODevice *addFile(const QString &path, int mode, qint64 length) = 0;
    };
    virtual int setupIncremental(int &cutoff) = 0;
    virtual void restoreLog() = 0;
    virtual ~Repository() {}

    virtual int createBranch(const QString &branch, int revnum,
			     const QString &branchFrom, int revFrom) = 0;
    virtual int deleteBranch(const QString &branch, int revnum) = 0;
    virtual Transaction *newTransaction(const QString &branch, const QString &svnprefix, int revnum) = 0;

    virtual void createAnnotatedTag(const QString &name, const QString &svnprefix, int revnum,
				    const QByteArray &author, uint dt,
				    const QByteArray &log) = 0;
    virtual void finalizeTags() = 0;
};

Repository *makeRepository(const Rules::Repository &rule, const QHash<QString, Repository *> &repositories);

#endif
