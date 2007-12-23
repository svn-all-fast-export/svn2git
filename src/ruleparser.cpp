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

#include <QTextStream>
#include <QFile>
#include <QDebug>

#include "ruleparser.h"

Rules::Rules(const QString &fn)
    : filename(fn)
{
}

Rules::~Rules()
{
}

QList<Rules::Repository> Rules::repositories()
{
    return m_repositories;
}

QList<Rules::Match> Rules::matchRules()
{
    return m_matchRules;
}

void Rules::load()
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return;

    // initialize the regexps we will use
    QRegExp repoLine("create repository\\s+(\\w+)", Qt::CaseInsensitive);
    QRegExp repoBranchLine("branch\\s+(\\S+)\\s+from\\s+(\\S+)", Qt::CaseInsensitive);
    QRegExp matchLine("match\\s+(.*)", Qt::CaseInsensitive);
    QRegExp matchRepoLine("repository\\s+(\\w+)", Qt::CaseInsensitive);
    QRegExp matchBranchLine("branch\\s+(\\S+)", Qt::CaseInsensitive);
    QRegExp matchPathLine("path\\s+(.*)", Qt::CaseInsensitive);

    QTextStream s(&file);
    enum { ReadingNone, ReadingRepository, ReadingMatch } state = ReadingNone;
    Repository repo;
    Match match;
    while (!s.atEnd()) {
        QString origLine = s.readLine();
        QString line = origLine.trimmed();

        int hash = line.indexOf('#');
        if (hash != -1) {
            line.truncate(hash);
            line = line.trimmed();
        }
        if (line.isEmpty())
            continue;

        if (state == ReadingRepository) {
            if (repoBranchLine.exactMatch(line)) {
                Repository::Branch branch;
                branch.name = repoBranchLine.cap(0);
                branch.branchFrom = repoBranchLine.cap(1);

                repo.branches += branch;
            }
        } else if (state == ReadingMatch) {
            if (matchRepoLine.exactMatch(line))
                match.repository = matchRepoLine.cap(0);
            else if (matchBranchLine.exactMatch(line))
                match.branch = matchBranchLine.cap(0);
            else if (matchPathLine.exactMatch(line))
                match.path = matchPathLine.cap(0);
        }

        bool isRepositoryRule = repoLine.exactMatch(line);
        bool isMatchRule = matchLine.exactMatch(line);
        if (isRepositoryRule || isMatchRule) {
            // save the current rule
            if (state == ReadingRepository)
                m_repositories += repo;
            else if (state == ReadingMatch)
                m_matchRules += match;
        }

        if (isRepositoryRule) {
            // repository rule
            state = ReadingRepository;
            repo = Repository(); // clear
            repo.name = repoLine.cap(0);
        } else if (isMatchRule) {
            // match rule
            state = ReadingMatch;
            match = Match();
            match.rx = QRegExp(matchLine.cap(0), Qt::CaseSensitive, QRegExp::RegExp2);
        } else {
            qWarning() << "Malformed line in configure file:" << origLine;
            state = ReadingNone;
        }
    }
}
