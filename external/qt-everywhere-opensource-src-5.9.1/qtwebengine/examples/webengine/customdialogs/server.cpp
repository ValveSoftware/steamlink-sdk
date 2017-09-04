/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "server.h"
#include <QDataStream>
#include <QTcpSocket>

Server::Server(QObject *parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &Server::handleNewConnection);
}

void Server::run()
{
    if (!m_server.listen(QHostAddress::LocalHost, 5555))
        qWarning() << "Could not start the server -> http/proxy authentication dialog"
                      " will not work. Error:" << m_server.errorString();
}

void Server::handleNewConnection()
{
    QTcpSocket *socket = m_server.nextPendingConnection();
    connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QAbstractSocket::readyRead, this, &Server::handleReadReady);
}

void Server::handleReadReady()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    Q_ASSERT(socket);
    QByteArray msg = socket->readAll();
    if (msg.contains(QByteArrayLiteral("OPEN_AUTH")))
        socket->write("HTTP/1.1 401 Unauthorized\nWWW-Authenticate: "
                      "Basic realm=\"Very Restricted Area\"\r\n\r\n");
    if (msg.contains(QByteArrayLiteral("OPEN_PROXY")))
        socket->write("HTTP/1.1 407 Proxy Auth Required\nProxy-Authenticate: "
                      "Basic realm=\"Proxy requires authentication\"\r\n\r\n");
}
