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

#include "ruleparser.h"
#include "repository.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList arguments = app.arguments();
    if (arguments.count() < 3) {
        printf("Usage: svn-all-fast-export configfile path-to-svn\n");
        return 0;
    }

    // Load the configuration
    Rules rules(arguments.at(1));
    rules.load();

    // create the repository list
    QHash<QString, Repository *> repositories;
    foreach (Rules::Repository rule, rules.repositories())
        repositories.insert(rule.name, new Repository(rule));

    // success
    return 0;
}
