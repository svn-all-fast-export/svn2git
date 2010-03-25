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

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <stdio.h>

#include "CommandLineParser.h"
#include "ruleparser.h"
#include "repository.h"
#include "svn.h"

QHash<QByteArray, QByteArray> loadIdentityMapFile(const QString &fileName)
{
    QHash<QByteArray, QByteArray> result;
    if (fileName.isEmpty())
        return result;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "Could not open file %s: %s",
                qPrintable(fileName), qPrintable(file.errorString()));
        return result;
    }

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

static const CommandLineOption options[] = {
    {"--identity-map FILENAME", "provide map between svn username and email"},
    {"--rules FILENAME", "the rules file that determines what goes where"},
    {"--add-metadata", "if passed, each git commit will have svn commit info"},
    {"--resume-from revision", "start importing at svn revision number"},
    {"--max-rev revision", "stop importing at svn revision number"},
    {"--dry-run", "don't actually write anything"},
    {"--debug-rules", "print what rule is being used for each file"},
    {"--commit-interval NUMBER", "if passed the cache will be flushed to git every NUMBER of commits"},
    {"-h, --help", "show help"},
    {"-v, --version", "show version"},
    CommandLineLastOption
};

int main(int argc, char **argv)
{
    CommandLineParser::init(argc, argv);
    CommandLineParser::addOptionDefinitions(options);
    CommandLineParser *args = CommandLineParser::instance();
    if (args->contains(QLatin1String("help")) || args->arguments().count() != 1) {
        args->usage(QString(), "[Path to subversion repo]");
        return 0;
    }
    if (args->undefinedOptions().count()) {
        QTextStream out(stderr);
        out << "svn-all-fast-export failed: ";
        bool first = true;
        foreach (QString option, args->undefinedOptions()) {
            if (!first)
                out << "          : ";
            out << "unrecognized option or missing argument for; `" << option << "'" << endl;
            first = false;
        }
        return 10;
    }
    if (!args->contains("rules")) {
        QTextStream out(stderr);
        out << "svn-all-fast-export failed: please specify the rules using the 'rules' argument\n";
        return 11;
    }
    if (!args->contains("identity-map")) {
        QTextStream out(stderr);
        out << "WARNING; no identity-map specified, all commits will be without email address\n\n";
    }

    QCoreApplication app(argc, argv);
    // Load the configuration
    Rules rules(args->optionArgument(QLatin1String("rules")));
    rules.load();

    int min_rev = args->optionArgument(QLatin1String("resume-from")).toInt();
    int max_rev = args->optionArgument(QLatin1String("max-rev")).toInt();
    if (min_rev < 1)
        min_rev = 1;

    // create the repository list
    QHash<QString, Repository *> repositories;
    foreach (Rules::Repository rule, rules.repositories()) {
        Repository *repo = new Repository(rule);
        repositories.insert(rule.name, repo);
    }

    Svn::initialize();
    Svn svn(args->arguments().first());
    svn.setMatchRules(rules.matchRules());
    svn.setRepositories(repositories);
    svn.setIdentityMap(loadIdentityMapFile(args->optionArgument("identity-map")));

    if (max_rev < 1)
        max_rev = svn.youngestRevision();
    for (int i = min_rev; i <= max_rev; ++i)
        if (!svn.exportRevision(i))
            break;

    foreach (Repository *repo, repositories) {
        repo->finalizeTags();
        delete repo;
    }

    // success
    return 0;
}
