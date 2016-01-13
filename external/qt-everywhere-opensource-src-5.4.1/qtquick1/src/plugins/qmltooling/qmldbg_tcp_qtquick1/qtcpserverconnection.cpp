/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qtcpserverconnection.h"

#include <QtCore/qplugin.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>

#include <private/qdeclarativedebugserver_p.h>
#include <private/qpacketprotocol_p.h>

QT_BEGIN_NAMESPACE

class QTcpServerConnectionPrivate {
public:
    QTcpServerConnectionPrivate();

    int port;
    bool block;
    QTcpSocket *socket;
    QPacketProtocol *protocol;
    QTcpServer *tcpServer;

    QDeclarativeDebugServer *debugServer;
};

QTcpServerConnectionPrivate::QTcpServerConnectionPrivate() :
    port(0),
    block(false),
    socket(0),
    protocol(0),
    tcpServer(0),
    debugServer(0)
{
}

QTcpServerConnection::QTcpServerConnection() :
    d_ptr(new QTcpServerConnectionPrivate)
{

}

QTcpServerConnection::~QTcpServerConnection()
{
    delete d_ptr;
}

void QTcpServerConnection::setServer(QDeclarativeDebugServer *server)
{
    Q_D(QTcpServerConnection);
    d->debugServer = server;
}

bool QTcpServerConnection::isConnected() const
{
    Q_D(const QTcpServerConnection);
    return d->socket && d->socket->state() == QTcpSocket::ConnectedState;
}

void QTcpServerConnection::send(const QList<QByteArray> &messages)
{
    Q_D(QTcpServerConnection);

    if (!isConnected()
            || !d->protocol || !d->socket)
        return;

    foreach (const QByteArray &message, messages) {
        QPacket pack;
        pack.writeRawData(message.data(), message.length());
        d->protocol->send(pack);
    }
    d->socket->flush();
}

void QTcpServerConnection::disconnect()
{
    Q_D(QTcpServerConnection);

    // protocol might still be processing packages at this point
    d->protocol->deleteLater();
    d->protocol = 0;
    d->socket->deleteLater();
    d->socket = 0;
}

bool QTcpServerConnection::waitForMessage()
{
    Q_D(QTcpServerConnection);
    if (d->protocol->packetsAvailable() > 0) {
        QPacket packet = d->protocol->read();
        d->debugServer->receiveMessage(packet.data());
        return true;
    } else {
        return d->protocol->waitForReadyRead(-1);
    }
}

void QTcpServerConnection::setPort(int port, bool block)
{
    Q_D(QTcpServerConnection);
    d->port = port;
    d->block = block;

    listen();
    if (block)
        d->tcpServer->waitForNewConnection(-1);
}

void QTcpServerConnection::listen()
{
    Q_D(QTcpServerConnection);

    d->tcpServer = new QTcpServer(this);
    QObject::connect(d->tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    if (d->tcpServer->listen(QHostAddress::Any, d->port)) {
        qDebug("QDeclarativeDebugServer: Waiting for connection on port %d...", d->port);
    } else {
        qWarning("QDeclarativeDebugServer: Unable to listen on port %d", d->port);
    }
}


void QTcpServerConnection::readyRead()
{
    Q_D(QTcpServerConnection);
    if (!d->protocol)
        return;

    while (d->protocol->packetsAvailable() > 0) {
        QPacket packet = d->protocol->read();
        d->debugServer->receiveMessage(packet.data());
    }
}

void QTcpServerConnection::newConnection()
{
    Q_D(QTcpServerConnection);

    if (d->socket) {
        qWarning("QDeclarativeDebugServer: Another client is already connected");
        QTcpSocket *faultyConnection = d->tcpServer->nextPendingConnection();
        delete faultyConnection;
        return;
    }

    d->socket = d->tcpServer->nextPendingConnection();
    d->socket->setParent(this);
    d->protocol = new QPacketProtocol(d->socket, this);
    QObject::connect(d->protocol, SIGNAL(readyRead()), this, SLOT(readyRead()));

    if (d->block) {
        d->protocol->waitForReadyRead(-1);
    }
}

QT_END_NAMESPACE

