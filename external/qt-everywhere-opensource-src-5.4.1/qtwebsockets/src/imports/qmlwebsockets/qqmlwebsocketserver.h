/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebSocket module of the Qt Toolkit.
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
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
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

    quint16 port() const;
    void setPort(quint16 port);

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
    void portChanged(quint16 port);
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
    quint16 m_port;
    bool m_listen;
    bool m_accept;
    bool m_componentCompleted;

};

QT_END_NAMESPACE

#endif // QQMLWEBSOCKETSERVER_H
