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

#ifdef QT_WIDGETS_LIB
#include <QtWidgets/QApplication>
#else
#include <QtGui/QGuiApplication>
#endif
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtGui/QImageReader>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QMimeDatabase>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>

static QStringList imageNameFilters()
{
    QStringList result;
    QMimeDatabase mimeDatabase;
    foreach (const QByteArray &m, QImageReader::supportedMimeTypes()) {
        foreach (const QString &suffix, mimeDatabase.mimeTypeForName(m).suffixes())
            result.append(QStringLiteral("*.") + suffix);
    }
    return result;
}

int main(int argc, char* argv[])
{
    // The reason to use QApplication is that QWidget-based dialogs
    // are the native dialogs on Qt-based platforms like KDE,
    // but they cannot be instantiated if this is a QGuiApplication.
#ifdef QT_WIDGETS_LIB
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    QQuickWindow::setDefaultAlphaBuffer(true);

    QCoreApplication::setApplicationName(QStringLiteral("Photosurface"));
    QCoreApplication::setOrganizationName(QStringLiteral("QtProject"));
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt Quick Demo - Photo Surface"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("directory"),
                                 QStringLiteral("The image directory or URL to show."));
    parser.process(app);

    QUrl initialUrl;
    if (!parser.positionalArguments().isEmpty()) {
        initialUrl = QUrl::fromUserInput(parser.positionalArguments().first(),
                                         QDir::currentPath(), QUrl::AssumeLocalFile);
        if (!initialUrl.isValid()) {
            qWarning().nospace() << "Invalid argument: \""
                << parser.positionalArguments().first() << "\": " << initialUrl.errorString();
            return 1;
        }
    }

    const QStringList nameFilters = imageNameFilters();

    QQmlApplicationEngine engine;
    QQmlContext *context = engine.rootContext();

    QUrl picturesLocationUrl = QUrl::fromLocalFile(QDir::homePath());
    const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!picturesLocations.isEmpty()) {
        picturesLocationUrl = QUrl::fromLocalFile(picturesLocations.first());
        if (initialUrl.isEmpty()
            && !QDir(picturesLocations.first()).entryInfoList(nameFilters, QDir::Files).isEmpty()) {
            initialUrl = picturesLocationUrl;
        }
    }

    context->setContextProperty(QStringLiteral("contextPicturesLocation"), picturesLocationUrl);
    context->setContextProperty(QStringLiteral("contextInitialUrl"), initialUrl);
    context->setContextProperty(QStringLiteral("contextImageNameFilters"), nameFilters);

    engine.load(QUrl("qrc:///photosurface.qml"));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
