/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "jsongenerator.h"
#include "logging.h"
#include "packagefilter.h"
#include "qdocgenerator.h"
#include "scanner.h"

#include <QtCore/qcommandlineparser.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <iostream>


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("Qt Attributions Scanner"));
    a.setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(tr("Searches and processes qt_attribution.json files in Qt sources."));
    parser.addPositionalArgument(QStringLiteral("directory"),
                                 tr("The directory to scan recursively."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption generatorOption(QStringLiteral("output-format"),
                                       tr("Output format (\"qdoc\", \"json\")."),
                                       QStringLiteral("generator"),
                                       QStringLiteral("qdoc"));
    QCommandLineOption filterOption(QStringLiteral("filter"),
                                    tr("Filter packages according to <filter> (e.g. QDocModule=qtcore)"),
                                    QStringLiteral("expression"));
    QCommandLineOption outputOption({ QStringLiteral("o"), QStringLiteral("output") },
                                    tr("Write generated data to <file>."),
                                    QStringLiteral("file"));
    QCommandLineOption verboseOption(QStringLiteral("verbose"),
                                     tr("Verbose output."));
    QCommandLineOption silentOption({ QStringLiteral("s"), QStringLiteral("silent") },
                                    tr("Minimal output."));

    parser.addOption(generatorOption);
    parser.addOption(filterOption);
    parser.addOption(outputOption);
    parser.addOption(verboseOption);
    parser.addOption(silentOption);

    parser.process(a.arguments());

    LogLevel logLevel = NormalLog;
    if (parser.isSet(verboseOption) && parser.isSet(silentOption)) {
        std::cerr << qPrintable(tr("--verbose and --silent cannot be set simultaneously.")) << std::endl;
        parser.showHelp(1);
    }

    if (parser.isSet(verboseOption))
        logLevel = VerboseLog;
    else if (parser.isSet(silentOption))
        logLevel = SilentLog;

    if (parser.positionalArguments().size() != 1)
        parser.showHelp(2);

    const QString directory = parser.positionalArguments().last();

    if (logLevel == VerboseLog)
        std::cerr << qPrintable(tr("Recursively scanning %1 for qt_attribution.json files...").arg(
                                    QDir::toNativeSeparators(directory))) << std::endl;

    QVector<Package> packages = Scanner::scanDirectory(directory, logLevel);

    if (parser.isSet(filterOption)) {
        PackageFilter filter(parser.value(filterOption));
        if (filter.type == PackageFilter::InvalidFilter)
            return 4;
        packages.erase(std::remove_if(packages.begin(), packages.end(),
                                      [&filter](const Package &p) { return !filter(p); }),
                       packages.end());
    }

    if (logLevel == VerboseLog)
        std::cerr << qPrintable(tr("%1 packages found.").arg(packages.size())) << std::endl;

    QTextStream out(stdout);
    QFile outFile(parser.value(outputOption));
    if (!outFile.fileName().isEmpty()) {
        if (!outFile.open(QFile::WriteOnly)) {
            std::cerr << qPrintable(tr("Cannot open %1 for writing.").arg(
                                        QDir::toNativeSeparators(outFile.fileName())))
                      << std::endl;
            return 5;
        }
        out.setDevice(&outFile);
    }

    QString generator = parser.value(generatorOption);
    if (generator == QLatin1String("qdoc")) {
        // include top level module name in printed paths
        QString baseDirectory = QDir(directory).absoluteFilePath(QStringLiteral(".."));
        QDocGenerator::generate(out, packages, baseDirectory, logLevel);
    } else if (generator == QLatin1String("json")) {
        JsonGenerator::generate(out, packages, logLevel);
    } else {
        std::cerr << qPrintable(tr("Unknown output-format %1.").arg(generator)) << std::endl;
        return 6;
    }

    if (logLevel == VerboseLog)
        std::cerr << qPrintable(tr("Processing is done.")) << std::endl;

    return 0;
}
