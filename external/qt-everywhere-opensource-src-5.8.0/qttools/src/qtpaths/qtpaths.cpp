/****************************************************************************
 * *
 ** Copyright (C) 2016 Sune Vuorela <sune@kde.org>
 ** Contact: http://www.qt-project.org/
 **
 ** This file is part of the tools applications of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:BSD$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and The Qt Company. For licensing terms
 ** and conditions see https://www.qt.io/terms-conditions. For further
 ** information use the contact form at https://www.qt.io/contact-us.
 **
 ** BSD License Usage
 ** Alternatively, you may use this file under the terms of the BSD license
 ** as follows:
 **
 ** "Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **   * Redistributions of source code must retain the above copyright
 **     notice, this list of conditions and the following disclaimer.
 **   * Redistributions in binary form must reproduce the above copyright
 **     notice, this list of conditions and the following disclaimer in
 **     the documentation and/or other materials provided with the
 **     distribution.
 **   * Neither the name of The Qt Company Ltd nor the names of its
 **     contributors may be used to endorse or promote products derived
 **     from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QHash>
#include <QLibraryInfo>

#include <stdio.h>

QT_USE_NAMESPACE

/**
 * Prints the string on stdout and appends a newline
 * \param string printable string
 */
static void message(const QString &string)
{
    fprintf(stdout, "%s\n", qPrintable(string));
}

/**
 * Writes error message and exits 1
 * \param message to write
 */
Q_NORETURN static void error(const QString &message)
{
    fprintf(stderr, "%s\n", qPrintable(message));
    ::exit(EXIT_FAILURE);
}


/*
 * NOTE: that DataLocation and CacheLocation are missing as
 * they don't really make sense for a utility like this because
 * they include the application name.
 */
static const struct StringEnum {
    const char *stringvalue;
    QStandardPaths::StandardLocation enumvalue;
} lookupTableData[] = {
    { "ApplicationsLocation", QStandardPaths::ApplicationsLocation },
    { "DesktopLocation", QStandardPaths::DesktopLocation },
    { "DocumentsLocation", QStandardPaths::DocumentsLocation },
    { "FontsLocation", QStandardPaths::FontsLocation },
    { "MusicLocation", QStandardPaths::MusicLocation },
    { "MoviesLocation", QStandardPaths::MoviesLocation },
    { "PicturesLocation", QStandardPaths::PicturesLocation },
    { "HomeLocation", QStandardPaths::HomeLocation },
    { "GenericCacheLocation", QStandardPaths::GenericCacheLocation },
    { "GenericDataLocation", QStandardPaths::GenericDataLocation },
    { "RuntimeLocation", QStandardPaths::RuntimeLocation },
    { "ConfigLocation", QStandardPaths::ConfigLocation },
    { "GenericConfigLocation", QStandardPaths::GenericConfigLocation },
    { "DownloadLocation", QStandardPaths::DownloadLocation }
};

/**
 * \return available types as a QStringList.
 */
static QStringList types()
{
    QStringList typelist;
    for (unsigned int i = 0; i < sizeof(lookupTableData)/sizeof(lookupTableData[0]); i++)
        typelist << QString::fromLatin1(lookupTableData[i].stringvalue);
    qSort(typelist);
    return typelist;
}

/**
 * Tries to parse the location string into a StandardLocation or alternatively
 * calls \ref error with a error message
 */
static QStandardPaths::StandardLocation parseLocationOrError(const QString &locationString)
{
    for (unsigned int i = 0; i < sizeof(lookupTableData)/sizeof(lookupTableData[0]); i++)
        if (locationString == QLatin1String(lookupTableData[i].stringvalue))
            return lookupTableData[i].enumvalue;

    QString message = QCoreApplication::translate("qtpaths", "Unknown location: %1");
    error(message.arg(locationString));
}

/**
 * searches for exactly one remaining argument and returns it.
 * If not found, \ref error is called with a error message.
 * \param parser to ask for remaining arguments
 * \return one extra argument
 */
static QString searchStringOrError(QCommandLineParser *parser)
{
    int positionalArgumentCount = parser->positionalArguments().size();
    if (positionalArgumentCount != 1)
        error(QCoreApplication::translate("qtpaths", "Exactly one argument needed as searchitem"));
    return parser->positionalArguments().first();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("1.0");

#ifdef Q_OS_WIN
    const QLatin1Char pathsep(';');
#else
    const QLatin1Char pathsep(':');
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("qtpaths", "Command line client to QStandardPaths"));
    parser.addPositionalArgument(QCoreApplication::translate("qtpaths", "[name]"), QCoreApplication::tr("Name of file or directory"));
    parser.addHelpOption();
    parser.addVersionOption();

    //setting up options
    QCommandLineOption types(QStringLiteral("types"), QCoreApplication::translate("qtpaths", "Available location types."));
    parser.addOption(types);

    QCommandLineOption paths(QStringLiteral("paths"), QCoreApplication::translate("qtpaths", "Find paths for <type>."), QStringLiteral("type"));
    parser.addOption(paths);

    QCommandLineOption writablePath(QStringLiteral("writable-path"),
                                    QCoreApplication::translate("qtpaths", "Find writable path for <type>."), QStringLiteral("type"));
    parser.addOption(writablePath);

    QCommandLineOption locateDir(QStringList() << QStringLiteral("locate-dir") << QStringLiteral("locate-directory"),
                                 QCoreApplication::translate("qtpaths", "Locate directory [name] in <type>."), QStringLiteral("type"));
    parser.addOption(locateDir);

    QCommandLineOption locateDirs(QStringList() << QStringLiteral("locate-dirs") << QStringLiteral("locate-directories"),
                                  QCoreApplication::translate("qtpaths", "Locate directories [name] in all paths for <type>."), QStringLiteral("type"));
    parser.addOption(locateDirs);

    QCommandLineOption locateFile(QStringLiteral("locate-file"),
                                  QCoreApplication::translate("qtpaths", "Locate file [name] for <type>."), QStringLiteral("type"));
    parser.addOption(locateFile);

    QCommandLineOption locateFiles(QStringLiteral("locate-files"),
                                   QCoreApplication::translate("qtpaths", "Locate files [name] in all paths for <type>."), QStringLiteral("type"));
    parser.addOption(locateFiles);

    QCommandLineOption findExe(QStringList() << QStringLiteral("find-exe") << QStringLiteral("find-executable"),
                               QCoreApplication::translate("qtpaths", "Find executable with [name]."));
    parser.addOption(findExe);

    QCommandLineOption display(QStringList() << QStringLiteral("display"),
                               QCoreApplication::translate("qtpaths", "Prints user readable name for <type>."), QStringLiteral("type"));
    parser.addOption(display);

    QCommandLineOption testmode(QStringList() << QStringLiteral("testmode") << QStringLiteral("test-mode"),
                                QCoreApplication::translate("qtpaths", "Use paths specific for unit testing."));
    parser.addOption(testmode);

    QCommandLineOption qtversion(QStringLiteral("qt-version"), QCoreApplication::translate("qtpaths", "Qt version."));
    parser.addOption(qtversion);

    QCommandLineOption installprefix(QStringLiteral("install-prefix"), QCoreApplication::translate("qtpaths", "Installation prefix for Qt."));
    parser.addOption(installprefix);

    QCommandLineOption bindir(QStringList() << QStringLiteral("binaries-dir") << QStringLiteral("binaries-directory"),
                              QCoreApplication::translate("qtpaths", "Location of Qt executables."));
    parser.addOption(bindir);

    QCommandLineOption plugindir(QStringList() << QStringLiteral("plugin-dir") << QStringLiteral("plugin-directory"),
                                 QCoreApplication::translate("qtpaths", "Location of Qt plugins."));
    parser.addOption(plugindir);

    parser.process(app);

    QStandardPaths::enableTestMode(parser.isSet(testmode));

    QStringList results;
    if (parser.isSet(qtversion)) {
        QString qtversionstring = QString::fromLatin1(qVersion());
        results << qtversionstring;
    }

    if (parser.isSet(installprefix)) {
        QString path = QLibraryInfo::location(QLibraryInfo::PrefixPath);
        results << path;
    }

    if (parser.isSet(bindir)) {
        QString path = QLibraryInfo::location(QLibraryInfo::BinariesPath);
        results << path;
    }

    if (parser.isSet(plugindir)) {
        QString path = QLibraryInfo::location(QLibraryInfo::PluginsPath);
        results << path;
    }

    if (parser.isSet(types)) {
        QStringList typesList = ::types();
        results << typesList.join('\n');
    }

    if (parser.isSet(display)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(display));
        QString text = QStandardPaths::displayName(location);
        results << text;
    }

    if (parser.isSet(paths)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(paths));
        QStringList paths = QStandardPaths::standardLocations(location);
        results << paths.join(pathsep);
    }

    if (parser.isSet(writablePath)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(writablePath));
        QString path = QStandardPaths::writableLocation(location);
        results << path;
    }

    if (parser.isSet(findExe)) {
        QString searchitem = searchStringOrError(&parser);
        QString path = QStandardPaths::findExecutable(searchitem);
        results << path;
    }

    if (parser.isSet(locateDir)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(locateDir));
        QString searchitem = searchStringOrError(&parser);
        QString path = QStandardPaths::locate(location, searchitem, QStandardPaths::LocateDirectory);
        results << path;
    }

    if (parser.isSet(locateFile)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(locateFile));
        QString searchitem = searchStringOrError(&parser);
        QString path = QStandardPaths::locate(location, searchitem, QStandardPaths::LocateFile);
        results << path;
    }

    if (parser.isSet(locateDirs)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(locateDirs));
        QString searchitem = searchStringOrError(&parser);
        QStringList paths = QStandardPaths::locateAll(location, searchitem, QStandardPaths::LocateDirectory);
        results << paths.join(pathsep);
    }

    if (parser.isSet(locateFiles)) {
        QStandardPaths::StandardLocation location = parseLocationOrError(parser.value(locateFiles));
        QString searchitem = searchStringOrError(&parser);
        QStringList paths = QStandardPaths::locateAll(location, searchitem, QStandardPaths::LocateFile);
        results << paths.join(pathsep);
    }
    if (results.isEmpty()) {
        parser.showHelp();
    } else if (results.size() == 1) {
        const QString &item = results.first();
        message(item);
        if (item.isEmpty())
            return EXIT_FAILURE;
    } else {
        QString errorMessage = QCoreApplication::translate("qtpaths", "Several options given, only one is supported at a time.");
        error(errorMessage);
    }
    return EXIT_SUCCESS;
}
