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

#include "ftpcontrolchannel.h"

FtpControlChannel::FtpControlChannel(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QIODevice::readyRead, this, &FtpControlChannel::onReadyRead);
    connect(&m_socket, &QAbstractSocket::disconnected, this, &FtpControlChannel::closed);
    connect(&m_socket, &QAbstractSocket::connected, this, [this]() {
        emit opened(m_socket.localAddress(), m_socket.localPort());
    });
}

void FtpControlChannel::connectToServer(const QString &server)
{
    m_socket.connectToHost(server, 21);
}

void FtpControlChannel::command(const QByteArray &command, const QByteArray &params)
{
    QByteArray sendData = command;
    if (!params.isEmpty())
        sendData += " " + params;
    m_socket.write(sendData + "\r\n");
}

void FtpControlChannel::onReadyRead()
{
    m_buffer.append(m_socket.readAll());
    int rn = -1;
    while ((rn = m_buffer.indexOf("\r\n")) != -1) {
        QByteArray received = m_buffer.mid(0, rn);
        m_buffer = m_buffer.mid(rn + 2);
        int space = received.indexOf(' ');
        if (space != -1) {
            int code = received.mid(0, space).toInt();
            if (code == 0)
                emit info(received.mid(space + 1));
            else
                emit reply(code, received.mid(space + 1));
        } else {
            emit invalidReply(received);
        }
    }
}
