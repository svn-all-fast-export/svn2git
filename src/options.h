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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <QHash>
#include <QString>

class Options
{
public:
    // mandatory non-option arguments
    QString ruleFile;
    QString pathToRepository;

    // optional extras
    QHash<QString, QString> options;
    QHash<QString, bool> switches;

    Options();
    ~Options();

    void showHelp();
    void parseArguments(const QStringList &arguments);

    static Options *globalOptions;
};

#endif
