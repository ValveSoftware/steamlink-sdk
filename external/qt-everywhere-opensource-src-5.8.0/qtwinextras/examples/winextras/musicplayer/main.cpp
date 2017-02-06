/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

#include "musicplayer.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QIcon>
#include <QDir>
#include <QUrl>

//! [0]
static bool associateFileTypes()
{
    QString displayName = QGuiApplication::applicationDisplayName();
    QString filePath = QCoreApplication::applicationFilePath();
    QString fileName = QFileInfo(filePath).fileName();

    const QString key = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\") + fileName;
    QSettings settings(key, QSettings::NativeFormat);
    if (settings.status() != QSettings::NoError) {
        qWarning() << "Cannot access registry key" << key;
        return false;
    }
    settings.setValue(QStringLiteral("FriendlyAppName"), displayName);

    settings.beginGroup(QStringLiteral("SupportedTypes"));
    QMimeDatabase mimeDatabase;
    foreach (const QString &fileType, MusicPlayer::supportedMimeTypes()) {
        foreach (QString suffix, mimeDatabase.mimeTypeForName(fileType).suffixes()) {
            suffix.prepend('.');
            settings.setValue(suffix, QString());
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("shell"));
    settings.beginGroup(QStringLiteral("open"));
    settings.setValue(QStringLiteral("FriendlyAppName"), displayName);
    settings.beginGroup(QStringLiteral("Command"));
    settings.setValue(QStringLiteral("."),
                      QLatin1Char('"') + QDir::toNativeSeparators(filePath) + QStringLiteral("\" \"%1\""));

    return true;
}
//! [0]

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MusicPlayer"));
    QCoreApplication::setApplicationVersion( QLatin1String(QT_VERSION_STR));
    QCoreApplication::setOrganizationName(QStringLiteral("QtWinExtras"));
    QCoreApplication::setOrganizationDomain("qt-project.org");
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QtWinExtras Music Player"));

    if (!associateFileTypes())
        return -1;

    QCommandLineParser parser;
    parser.setApplicationDescription(QGuiApplication::applicationDisplayName());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("url"), MusicPlayer::tr("The URL to open."));
    parser.process(app);

    MusicPlayer player;

    if (!parser.positionalArguments().isEmpty())
        player.playUrl(QUrl::fromUserInput(parser.positionalArguments().constFirst(), QDir::currentPath(), QUrl::AssumeLocalFile));

    const QRect availableGeometry = QApplication::desktop()->availableGeometry(&player);
    player.resize(availableGeometry.width() / 6, availableGeometry.height() / 17);
    player.show();

    return app.exec();
}
