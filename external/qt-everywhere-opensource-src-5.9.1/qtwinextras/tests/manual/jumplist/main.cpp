/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the test suite of the Qt Toolkit.
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

#include "testwidget.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QDebug>
#include <QMimeDatabase>
#include <QSettings>
#include <QStatusBar>

#include <algorithm>
#include <iterator>

static bool associateFileType()
{
    const QString applicationBinary = QCoreApplication::applicationFilePath();
    QString exeFileName = applicationBinary;
    const int lastSlashPos = exeFileName.lastIndexOf(QLatin1Char('/'));
    exeFileName.remove(0, lastSlashPos + 1);
    QSettings regApplications("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\" + exeFileName, QSettings::NativeFormat);
    regApplications.setValue("FriendlyAppName", QGuiApplication::applicationDisplayName());

    regApplications.beginGroup("SupportedTypes");
    QMimeDatabase mimeDatabase;
    foreach (const QString &t, TestWidget::supportedMimeTypes()) {
        foreach (const QString &s, mimeDatabase.mimeTypeForName(t).suffixes())
            regApplications.setValue('.' + s, QString());
    }
    regApplications.endGroup();

    regApplications.beginGroup("shell");
    regApplications.beginGroup("open");
    regApplications.setValue("FriendlyAppName", QGuiApplication::applicationDisplayName());
    regApplications.beginGroup("command");
    regApplications.setValue(".", '"' + QDir::toNativeSeparators(applicationBinary) + "\" \"%1\"");
    regApplications.endGroup();
    regApplications.endGroup();
    regApplications.endGroup();
    return regApplications.status() == QSettings::NoError;
}

int main(int argc, char *argv[])
{
    QStringList allArgs; // Show all arguments including style.
    std::copy(argv + 1, argv + argc, std::back_inserter(allArgs));

    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QtWinExtras JumpList Test"));
    if (!associateFileType()) {
        qWarning() << "Unable to create registry entries.";
        return -1;
    }

    QCoreApplication::setOrganizationName("QtProject");
    QCoreApplication::setApplicationVersion(QT_VERSION_STR);
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(QGuiApplication::applicationDisplayName());
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption textOption("text", "Show some text");
    parser.addOption(textOption);
    QCommandLineOption fullScreenOption("fullscreen", "Show fullscreen");
    parser.addOption(fullScreenOption);
    QCommandLineOption idOption("id", "Jump list identifier", "id");
    parser.addOption(idOption);
    parser.addPositionalArgument("file", "The file to open.");
    parser.process(app);

    TestWidget w;

    if (parser.isSet(idOption))
        w.setId(parser.value(idOption));

    if (parser.isSet(fullScreenOption))
        w.showFullScreen();
    else
        w.show();

    if (parser.isSet(textOption))
        w.setText("Hello, world!");

    if (!parser.positionalArguments().isEmpty())
        w.showFile(parser.positionalArguments().first());

    if (allArgs.isEmpty())
        w.statusBar()->showMessage("Remember to run windeployqt");
    else
        w.statusBar()->showMessage("Arguments: " + allArgs.join(' '));

    return app.exec();
}
