/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QScopedPointer>

#include <signal.h>

#include "canbusutil.h"
#include "sigtermhandler.h"

#define PROGRAMNAME "canbusutil"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral(PROGRAMNAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

    QScopedPointer<SigTermHandler> s(SigTermHandler::instance());
    if (signal(SIGINT, SigTermHandler::handle) == SIG_ERR)
        return -1;
    QObject::connect(s.data(), &SigTermHandler::sigTermSignal, &app, &QCoreApplication::quit);

    QTextStream output(stdout);
    CanBusUtil util(output, app);

    QCommandLineParser parser;
    parser.setApplicationDescription(CanBusUtil::tr(
        "Sends arbitrary CAN bus frames.\n"
        "If the -l option is set, all received CAN bus packages are dumped."));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("plugin"),
            CanBusUtil::tr("Plugin name to use. See --list-plugins."));

    parser.addPositionalArgument(QStringLiteral("device"),
            CanBusUtil::tr("Device to use."));

    parser.addPositionalArgument(QStringLiteral("data"),
            CanBusUtil::tr(
                "Data to send if -l is not specified. Format:\n"
                "\t\t<id>#{payload}   (CAN 2.0 data frames),\n"
                "\t\t<id>#Rxx         (CAN 2.0 RTR frames with xx bytes data length),\n"
                "\t\t<id>##{payload}  (CAN FD data frames),\n"
                "where {payload} has 0..8 (0..64 CAN FD) ASCII hex-value pairs, "
                "e.g. 1#1a2b3c\n"), QStringLiteral("[data]"));

    const QCommandLineOption listeningOption({"l", "listen"},
            CanBusUtil::tr("Start listening CAN data on device."));
    parser.addOption(listeningOption);

    const QCommandLineOption listOption({"L", "list-plugins"},
            CanBusUtil::tr("List all available plugins."));
    parser.addOption(listOption);

    const QCommandLineOption showTimeStampOption({"t", "timestamp"},
            CanBusUtil::tr("Show timestamp for each received message."));
    parser.addOption(showTimeStampOption);

    parser.process(app);

    if (parser.isSet(listOption)) {
        util.printPlugins();
        return 0;
    }

    QString data;
    const QStringList args = parser.positionalArguments();
    if (!parser.isSet(listeningOption) && args.size() == 3) {
        data = args[2];
    } else if (!parser.isSet(listeningOption) || args.size() != 2) {
        fprintf(stderr, "Invalid number of arguments (%d given).\n\n%s",
            args.size(), qPrintable(parser.helpText()));
        return 1;
    }

    util.setShowTimeStamp(parser.isSet(showTimeStampOption));
    if (!util.start(args[0], args[1], data))
        return -1;

    return app.exec();
}
