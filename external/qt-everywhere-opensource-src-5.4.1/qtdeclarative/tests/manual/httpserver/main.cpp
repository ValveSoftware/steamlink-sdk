/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <testhttpserver.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <iostream>
#include <iterator>
#include <algorithm>

inline std::wostream &operator<<(std::wostream &str, const QString &s)
{
#ifdef Q_OS_WIN
    str << reinterpret_cast<const wchar_t *>(s.utf16());
#else
    str << s.toStdWString();
#endif
    return str;
}

enum { defaultPort = 14457 };

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("HttpServer");
    QCoreApplication::setApplicationVersion(QT_VERSION_STR);

    QCommandLineParser commandLineParser;
    commandLineParser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    commandLineParser.setApplicationDescription(QStringLiteral("HTTP Test server"));
    commandLineParser.addHelpOption();
    commandLineParser.addVersionOption();

    QCommandLineOption portOption(QStringLiteral("p"),
                                 QStringLiteral("Port (default: ") + QString::number(defaultPort) + QLatin1Char(')'),
                                 QStringLiteral("port"));

    commandLineParser.addOption(portOption);

    commandLineParser.addPositionalArgument(QStringLiteral("[directory]"),
                                  QStringLiteral("Directory to serve."));

    commandLineParser.process(a);

    const QStringList args = commandLineParser.positionalArguments();
    if (args.size() != 1)
        commandLineParser.showHelp(1);

    const QString directory = QDir::cleanPath(args.front());
    if (!QFileInfo(directory).isDir()) {
        std::wcerr << '"' << QDir::toNativeSeparators(directory) <<  "\" is not a directory.\n";
        return -1;
    }

    unsigned short port = defaultPort;
    if (commandLineParser.isSet(portOption)) {
        const QString portV = commandLineParser.value(portOption);
        bool ok;
        port = portV.toUShort(&ok);
        if (!ok) {
            std::wcerr << portV << " is not a valid port number.\n";
            return -1;
        }
    }

    std::wcout << "Serving \"" << QDir::toNativeSeparators(directory)
               << "\":\n\n" << QDir(directory).entryList(QDir::Files).join(QLatin1Char('\n'))
               << "\n\non http://localhost:" << port << '\n';

    TestHTTPServer server;
    if (!server.listen(port)) {
        std::wcout << "Couldn't listen on port " << port << server.errorString().toLocal8Bit();
        exit(-1);
    }
    server.serveDirectory(directory);

    return a.exec();
}
