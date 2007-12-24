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

#ifndef RULEPARSER_H
#define RULEPARSER_H

#include <QList>
#include <QRegExp>
#include <QString>

class Rules
{
public:
    struct Repository
    {
        struct Branch
        {
            QString name;
        };

        QString name;
        QList<Branch> branches;
        int lineNumber;

        Repository() : lineNumber(0) { }
    };

    struct Match
    {
        QRegExp rx;
        QString repository;
        QString branch;
        int minRevision;
        int maxRevision;
        int lineNumber;

        enum Action {
            Ignore,
            Export,
            Recurse
        } action;

        Match() : minRevision(-1), maxRevision(-1), lineNumber(0), action(Ignore) { }
    };

    Rules(const QString &filename);
    ~Rules();

    QList<Repository> repositories();
    QList<Match> matchRules();

    void load();

private:
    QString filename;
    QList<Repository> m_repositories;
    QList<Match> m_matchRules;
};

#endif
