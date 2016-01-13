/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkProxy>

#include <QQmlEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QtQuick/QQuickView>


/*
   This example illustrates using a QQmlNetworkAccessManagerFactory to
   create a QNetworkAccessManager with a proxy.

   Usage:
     networkaccessmanagerfactory [-host <proxy> -port <port>] [file]
*/

#ifndef QT_NO_NETWORKPROXY
static QString proxyHost;
static int proxyPort = 0;
#endif // !QT_NO_NETWORKPROXY

class MyNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    virtual QNetworkAccessManager *create(QObject *parent);
};

QNetworkAccessManager *MyNetworkAccessManagerFactory::create(QObject *parent)
{
    QNetworkAccessManager *nam = new QNetworkAccessManager(parent);
#ifndef QT_NO_NETWORKPROXY
    if (!proxyHost.isEmpty()) {
        qDebug() << "Created QNetworkAccessManager using proxy" << (proxyHost + ":" + QString::number(proxyPort));
        QNetworkProxy proxy(QNetworkProxy::HttpCachingProxy, proxyHost, proxyPort);
        nam->setProxy(proxy);
    }
#endif // !QT_NO_NETWORKPROXY

    return nam;
}

int main(int argc, char ** argv)
{
    QUrl source("qrc:view.qml");

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
#ifndef QT_NO_NETWORKPROXY
    QCommandLineOption proxyHostOption("host", "The proxy host to use.", "host");
    parser.addOption(proxyHostOption);
    QCommandLineOption proxyPortOption("port", "The proxy port to use.", "port", "0");
    parser.addOption(proxyPortOption);
#endif // !QT_NO_NETWORKPROXY
    parser.addPositionalArgument("file", "The file to use.");
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    QStringList arguments = QCoreApplication::arguments();
    if (!parser.parse(arguments)) {
        qWarning() << parser.helpText() << '\n' << parser.errorText();
        exit(1);
    }
    if (parser.isSet(helpOption)) {
        qWarning() << parser.helpText();
        exit(0);
    }
#ifndef QT_NO_NETWORKPROXY
    if (parser.isSet(proxyHostOption))
        proxyHost = parser.value(proxyHostOption);
    if (parser.isSet(proxyPortOption)) {
        bool ok = true;
        proxyPort = parser.value(proxyPortOption).toInt(&ok);
        if (!ok || proxyPort < 1 || proxyPort > 65535) {
            qWarning() << parser.helpText() << "\nNo valid port given. It should\
                          be a number between 1 and 65535";
            exit(1);
        }
    }
#endif // !QT_NO_NETWORKPROXY
    if (parser.positionalArguments().count() == 1)
        source = QUrl::fromLocalFile(parser.positionalArguments().first());

    QQuickView view;
    view.engine()->setNetworkAccessManagerFactory(new MyNetworkAccessManagerFactory);

    view.setSource(source);
    view.show();

    return app.exec();
}

