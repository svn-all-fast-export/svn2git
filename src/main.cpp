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

#include <QCoreApplication>
#include <QFile>
#include <QStringList>

#include <stdio.h>

#include "options.h"
#include "ruleparser.h"
#include "repository.h"
#include "svn.h"

QHash<QByteArray, QByteArray> loadIdentityMapFile(const QString &fileName)
{
    QHash<QByteArray, QByteArray> result;
    if (fileName.isEmpty())
        return result;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return result;

    while (!file.atEnd()) {
        QByteArray line = file.readLine().trimmed();
        int space = line.indexOf(' ');
        if (space == -1)
            continue;           // invalid line

        QByteArray realname = line.mid(space).trimmed();
        line.truncate(space);
        result.insert(line, realname);
    };

    return result;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Options options;
    options.parseArguments(app.arguments());

    // Load the configuration
    Rules rules(options.ruleFile);
    rules.load();

    int min_rev = options.options.value("resume-from").toInt();
    int max_rev = options.options.value("max-rev").toInt();
    if (min_rev < 1)
        min_rev = 1;

    // create the repository list
    QHash<QString, Repository *> repositories;
    foreach (Rules::Repository rule, rules.repositories()) {
        Repository *repo = new Repository(rule);
        repositories.insert(rule.name, repo);
    }

    Svn::initialize();
    Svn svn(options.pathToRepository);
    svn.setMatchRules(rules.matchRules());
    svn.setRepositories(repositories);
    svn.setIdentityMap(loadIdentityMapFile(options.options.value("identity-map")));

    if (max_rev < 1)
        max_rev = svn.youngestRevision();
    for (int i = min_rev; i <= max_rev; ++i)
        if (!svn.exportRevision(i))
            break;

    qDeleteAll(repositories);
    // success
    return 0;
}
