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
#include <QStringList>

#include <stdio.h>

#include "options.h"
#include "ruleparser.h"
#include "repository.h"
#include "svn.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Options options;
    options.parseArguments(app.arguments());

    // Load the configuration
    Rules rules(options.ruleFile);
    rules.load();

    // create the repository list
    QHash<QString, Repository *> repositories;
    foreach (Rules::Repository rule, rules.repositories())
        repositories.insert(rule.name, new Repository(rule));

    Svn::initialize();
    Svn svn(options.pathToRepository);
    svn.setMatchRules(rules.matchRules());
    svn.setRepositories(repositories);

    int max_rev = svn.youngestRevision();
    for (int i = 1; i <= max_rev; ++i)
        if (!svn.exportRevision(i))
            break;

    qDeleteAll(repositories);
    // success
    return 0;
}
