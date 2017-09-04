/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include "networkcontroller.h"

NetworkController::NetworkController(QObject *parent) :
    QObject(parent)
{
    QObject::connect(&m_server, &QTcpServer::newConnection, this, &NetworkController::newConnection);

    if (!m_server.listen(QHostAddress::Any, 8080)) {
        qDebug() << "Failed to run http server";
    }
}

void NetworkController::newConnection()
{
    QTcpSocket *socket = m_server.nextPendingConnection();

    if (!socket)
        return;

    QObject::connect(socket, &QAbstractSocket::disconnected, this, &NetworkController::disconnected);
    QObject::connect(socket, &QIODevice::readyRead, this, &NetworkController::readyRead);
}

void NetworkController::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    socket->disconnect();
    socket->deleteLater();
}

void NetworkController::readyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket->state() == QTcpSocket::ClosingState)
        return;

    QString requestData = socket->readAll();
    QStringList list = requestData.split(' ');
    QString path = list[1];
    list = path.split('/');

    QByteArray reply;
    if (list.count() == 3) {
        socket->write("HTTP/1.1 200 OK\r\n");
        reply = QStringLiteral("Command accepted: %1 %2").arg(list[1], list[2]).toUtf8();
        emit commandAccepted(list[1], list[2]);
    } else {
        socket->write("HTTP/1.1 404 Not Found\r\n");
        reply = "Command rejected";
    }

    socket->write("Content-Type: text/plain\r\n");
    socket->write(QStringLiteral("Content-Length: %1\r\n").arg(reply.size()).toUtf8());
    socket->write("Connection: close\r\n");
    socket->write("\r\n");
    socket->write(reply);
    socket->disconnectFromHost();
}
