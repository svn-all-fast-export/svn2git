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

#ifndef RULEPARSER_H
#define RULEPARSER_H

#include <QList>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QStringBuilder>

class Rules
{
public:
    struct Rule
    {
        QString filename;
        int lineNumber;
        Rule() : lineNumber(0) {}
    };
    struct Repository : Rule
    {
        struct Branch
        {
            QString name;
        };

        QString name;
        QList<Branch> branches;

        QString forwardTo;
        QString prefix;

        Repository() { }
        const QString info() const {
            const QString info = Rule::filename % ":" % QByteArray::number(Rule::lineNumber);
            return info;
        }

    };

    struct Match : Rule
    {
        QRegExp rx;
        QString repository;
        QString branch;
        QString prefix;
        int minRevision;
        int maxRevision;
        bool annotate;

        enum Action {
            Ignore,
            Export,
            Recurse
        } action;

        Match() : minRevision(-1), maxRevision(-1), annotate(false), action(Ignore) { }
        const QString info() const {
            const QString info = rx.pattern() % " (" % Rule::filename % ":" % QByteArray::number(Rule::lineNumber) % ")";
            return info;
        }
    };

    Rules(const QString &filename);
    ~Rules();

    const QList<Repository> repositories() const;
    const QList<Match> matchRules() const;

    void load();
    QStringList readRules(const QString &filename) const;

private:
    QString filename;
    QList<Repository> m_repositories;
    QList<Match> m_matchRules;
};

class RulesList
{
public:
  RulesList( const QString &filenames);
  ~RulesList();

  const QList<Rules::Repository> allRepositories() const;
  const QList<QList<Rules::Match> > allMatchRules() const;
  const QList<Rules*> rules() const;
  void load();

private:
  QString m_filenames;
  QList<Rules*> m_rules;
  QList<Rules::Repository> m_allrepositories;
  QList<QList<Rules::Match> > m_allMatchRules;
};

#ifndef QT_NO_DEBUG_STREAM
class QDebug;
QDebug operator<<(QDebug, const Rules::Match &);
#endif

#endif
