/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
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

#ifndef MODBUSTCPCLIENT_P_H
#define MODBUSTCPCLIENT_P_H

#include "modbustcpclient.h"

#include <QDebug>
#include <QModbusReply>

#include <private/qmodbustcpclient_p.h>

QT_USE_NAMESPACE

class ModbusTcpClientPrivate : private QModbusTcpClientPrivate
{
    Q_DECLARE_PUBLIC(ModbusTcpClient)

public:
    QModbusReply *enqueueRequest(const QModbusRequest &request, int, const QModbusDataUnit &unit,
                                 QModbusReply::ReplyType type) override
    {
        auto writeToSocket = [this](const QModbusRequest &request) {
            QByteArray buffer;
            QDataStream output(&buffer, QIODevice::WriteOnly);
            output << m_tId << m_pId << m_length << m_uId << request;

            int writtenBytes = m_socket->write(buffer);
            if (writtenBytes == -1 || writtenBytes < buffer.size()) {
                Q_Q(ModbusTcpClient);
                qDebug() << "Cannot write request to socket.";
                q->setError(QModbusTcpClient::tr("Could not write request to socket."),
                            QModbusDevice::WriteError);
                return false;
            }
            qDebug() << "Sent TCP ADU:" << buffer.toHex();
            qDebug() << "Sent TCP PDU:" << request << "with tId:" << hex << m_tId;
            return true;
        };

        if (!writeToSocket(request))
            return nullptr;

        Q_Q(ModbusTcpClient);
        auto reply = new QModbusReply(type, m_uId, q);
        const auto element = QueueElement{ reply, request, unit, m_numberOfRetries,
            m_responseTimeoutDuration };
        m_transactionStore.insert(m_tId, element);

        using TypeId = void (QTimer::*)(int);
        q->connect(q, &QModbusClient::timeoutChanged,
                   element.timer.data(), static_cast<TypeId>(&QTimer::setInterval));
        QObject::connect(element.timer.data(), &QTimer::timeout, [this, writeToSocket]() {
            if (!m_transactionStore.contains(m_tId))
                return;

            QueueElement elem = m_transactionStore.take(m_tId);
            if (elem.reply.isNull())
                return;

            if (elem.numberOfRetries > 0) {
                elem.numberOfRetries--;
                if (!writeToSocket(elem.requestPdu))
                    return;
                m_transactionStore.insert(m_tId, elem);
                elem.timer->start();
                qDebug() << "Resend request with tId:" << hex << m_tId;
            } else {
                qDebug() << "Timeout of request with tId:" << hex << m_tId;
                elem.reply->setError(QModbusDevice::TimeoutError,
                    QModbusClient::tr("Request timeout."));
            }
        });
        element.timer->start();
    return reply;
    }

    quint16 m_tId;
    quint16 m_pId;
    quint16 m_length;
    quint8 m_uId;
};

#endif // MODBUSTCPCLIENT_P_H
