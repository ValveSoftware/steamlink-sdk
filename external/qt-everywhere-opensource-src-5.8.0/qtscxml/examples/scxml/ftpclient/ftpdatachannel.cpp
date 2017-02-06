/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "ftpdatachannel.h"

FtpDataChannel::FtpDataChannel(QObject *parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
        m_socket.reset(m_server.nextPendingConnection());
        connect(m_socket.data(), &QTcpSocket::readyRead, [this]() {
            emit dataReceived(m_socket->readAll());
        });
    });
}

void FtpDataChannel::listen(const QHostAddress &address)
{
    m_server.listen(address);
}

void FtpDataChannel::sendData(const QByteArray &data)
{
    if (m_socket)
        m_socket->write(QByteArray(data).replace("\n", "\r\n"));
}

void FtpDataChannel::close()
{
    if (m_socket)
        m_socket->disconnectFromHost();
}

QString FtpDataChannel::portspec() const
{
    // Yes, this is a weird format, but say hello to FTP.
    QString portSpec;
    quint32 ipv4 = m_server.serverAddress().toIPv4Address();
    quint16 port = m_server.serverPort();
    portSpec += QString::number((ipv4 & 0xff000000) >> 24);
    portSpec += ',' + QString::number((ipv4 & 0x00ff0000) >> 16);
    portSpec += ',' + QString::number((ipv4 & 0x0000ff00) >> 8);
    portSpec += ',' + QString::number(ipv4 & 0x000000ff);
    portSpec += ',' + QString::number((port & 0xff00) >> 8);
    portSpec += ',' + QString::number(port &0x00ff);
    return portSpec;
}
