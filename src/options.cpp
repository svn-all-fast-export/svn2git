/*
 *  Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
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

#include "options.h"

#include <QSet>
#include <QStringList>

#include <stdio.h>
#include <stdlib.h>

Options* Options::globalOptions = 0;

Options::Options()
{
    globalOptions = this;
}

Options::~Options()
{
}

void Options::showHelp()
{
    printf("Usage: svn-all-fast-export configfile path-to-svn\n");
}

void Options::parseArguments(const QStringList &argumentList)
{
    QSet<QString> validOptions;
    validOptions << "help";

    QSet<QString> validOptionsWithComplement;
    validOptionsWithComplement << "resume-from" << "identity-map";

    QStringList arguments = argumentList;
    arguments.takeFirst();           // the first one is the executable name; drop it
    while (!arguments.isEmpty()) {
        QString arg = arguments.takeFirst();
        QString complement;

        if (arg == "--")
            break;

        if (arg.startsWith("--"))
            arg = arg.mid(1);   // drop double dashes to single

        if (arg.startsWith("-no-")) {
            complement = "no";
            arg = arg.mid(4);
        } else if (!arg.startsWith("-")) {
            // non-option arg
            arguments.prepend(arg);
            break;
        } else { // starts with "-"
            arg = arg.mid(1);
        }

        if (arg.contains('=') && complement.isEmpty()) {
            int pos = arg.indexOf('=');
            complement = arg.mid(pos + 1);
            arg.truncate(pos);
        }

        if (validOptionsWithComplement.contains(arg)) {
            if (arguments.isEmpty()) {
                fprintf(stderr, "Option -%s requires an argument\n", qPrintable(arg));
                exit(2);
            }

            if (options.contains(arg)) {
                fprintf(stderr, "Option -%s given more than once\n", qPrintable(arg));
                exit(2);
            }

            if (!complement.isEmpty())
                options[arg] = complement;
            else if (!arguments.isEmpty())
                options[arg] = arguments.takeFirst();
            else {
                fprintf(stderr, "Option -%s requires an argument\n", qPrintable(arg));
                exit(2);
            }
            continue;
        } else if (validOptions.contains(arg)) {
            if (switches.contains(arg)) {
                fprintf(stderr, "Option -%s given more than once\n", qPrintable(arg));
                exit(2);
            }

            switches[arg] = !(complement == "no");
        } else {
            if (complement == "no")
                fprintf(stderr, "Invalid option: -no-%s\n", qPrintable(arg));
            else
                fprintf(stderr, "Invalid option: -%s\n", qPrintable(arg));
            exit(2);
        }
    }

    if (switches.value("help")) {
        showHelp();
        exit(0);
    } else if (arguments.count() < 2) {
        showHelp();
        exit(2);
    }

    ruleFile = arguments.takeFirst();
    pathToRepository = arguments.takeFirst();
}
