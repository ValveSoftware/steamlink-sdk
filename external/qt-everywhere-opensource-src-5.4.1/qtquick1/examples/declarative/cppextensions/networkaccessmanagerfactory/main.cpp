/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QDeclarativeEngine>
#include <QDeclarativeNetworkAccessManagerFactory>
#include "qmlapplicationviewer.h"

/*
   This example illustrates using a QDeclarativeNetworkAccessManagerFactory to
   create a QNetworkAccessManager with a proxy.

   Usage:
     networkaccessmanagerfactory [-host <proxy> -port <port>] [file]
*/

static QString proxyHost;
static int proxyPort = 0;

class MyNetworkAccessManagerFactory : public QDeclarativeNetworkAccessManagerFactory
{
public:
    virtual QNetworkAccessManager *create(QObject *parent);
};

QNetworkAccessManager *MyNetworkAccessManagerFactory::create(QObject *parent)
{
    QNetworkAccessManager *nam = new QNetworkAccessManager(parent);
    if (!proxyHost.isEmpty()) {
        qDebug() << "Created QNetworkAccessManager using proxy" << (proxyHost + ":" + QString::number(proxyPort));
        QNetworkProxy proxy(QNetworkProxy::HttpCachingProxy, proxyHost, proxyPort);
        nam->setProxy(proxy);
    }

    return nam;
}

int main(int argc, char ** argv)
{
    QUrl source("qml/networkaccessmanagerfactory/view.qml");

    QApplication app(argc, argv);
    QmlApplicationViewer viewer;

    for (int i = 1; i < argc; ++i) {
        QString arg(argv[i]);
        if (arg == "-host" && i < argc-1) {
            proxyHost = argv[++i];
        } else if (arg == "-port" && i < argc-1) {
            arg = argv[++i];
            proxyPort = arg.toInt();
        } else if (arg[0] != '-') {
            source = QUrl::fromLocalFile(arg);
        } else {
            qWarning() << "Usage: networkaccessmanagerfactory [-host <proxy> -port <port>] [file]";
            exit(1);
        }
    }

    viewer.engine()->setNetworkAccessManagerFactory(new MyNetworkAccessManagerFactory);
    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationLockLandscape);
    viewer.setMainQmlFile(source.toString());
    viewer.showExpanded();

    return app.exec();
}

