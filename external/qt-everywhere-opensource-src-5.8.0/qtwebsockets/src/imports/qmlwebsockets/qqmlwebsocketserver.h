/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebSocket module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQMLWEBSOCKETSERVER_H
#define QQMLWEBSOCKETSERVER_H

#include <QUrl>
#include <QQmlParserStatus>
#include <QtWebSockets/QWebSocketServer>

QT_BEGIN_NAMESPACE

class QQmlWebSocket;
class QQmlWebSocketServer : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_DISABLE_COPY(QQmlWebSocketServer)
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(bool listen READ listen WRITE setListen NOTIFY listenChanged)
    Q_PROPERTY(bool accept READ accept WRITE setAccept NOTIFY acceptChanged)

public:
    explicit QQmlWebSocketServer(QObject *parent = Q_NULLPTR);
    virtual ~QQmlWebSocketServer();

    void classBegin() Q_DECL_OVERRIDE;
    void componentComplete() Q_DECL_OVERRIDE;

    QUrl url() const;

    QString host() const;
    void setHost(const QString &host);

    int port() const;
    void setPort(int port);

    QString name() const;
    void setName(const QString &name);

    QString errorString() const;

    bool listen() const;
    void setListen(bool listen);

    bool accept() const;
    void setAccept(bool accept);

Q_SIGNALS:
    void clientConnected(QQmlWebSocket *webSocket);

    void errorStringChanged(const QString &errorString);
    void urlChanged(const QUrl &url);
    void portChanged(int port);
    void nameChanged(const QString &name);
    void hostChanged(const QString &host);
    void listenChanged(bool listen);
    void acceptChanged(bool accept);

private:
    void init();
    void updateListening();
    void newConnection();
    void serverError();
    void closed();

    QScopedPointer<QWebSocketServer> m_server;
    QString m_host;
    QString m_name;
    int m_port;
    bool m_listen;
    bool m_accept;
    bool m_componentCompleted;

};

QT_END_NAMESPACE

#endif // QQMLWEBSOCKETSERVER_H
