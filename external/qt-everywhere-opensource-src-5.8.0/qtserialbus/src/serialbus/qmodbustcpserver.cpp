/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmodbustcpserver.h"
#include "qmodbustcpserver_p.h"

#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

/*!
    \class QModbusTcpServer
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusTcpServer class represents a Modbus server that uses a
    TCP server for its communication with the Modbus client.

    Communication via Modbus requires the interaction between a single Modbus
    client instance and single Modbus server. This class provides the Modbus
    server implementation via a TCP server.

    Modbus TCP networks can have multiple servers. Servers are read/written by
    a client device represented by \l QModbusTcpClient.
*/

/*!
    Constructs a QModbusTcpServer with the specified \a parent. The
    \l serverAddress preset is \c 255.
*/
QModbusTcpServer::QModbusTcpServer(QObject *parent)
    : QModbusServer(*new QModbusTcpServerPrivate, parent)
{
    Q_D(QModbusTcpServer);
    d->setupTcpServer();
    setServerAddress(0xff);
}

/*!
    Destroys the QModbusTcpServer instance.
*/
QModbusTcpServer::~QModbusTcpServer()
{
    close();
}

/*!
    \internal
*/
QModbusTcpServer::QModbusTcpServer(QModbusTcpServerPrivate &dd, QObject *parent)
    : QModbusServer(dd, parent)
{
    Q_D(QModbusTcpServer);
    d->setupTcpServer();
}

/*!
    \reimp
*/
bool QModbusTcpServer::open()
{
    if (state() == QModbusDevice::ConnectedState)
        return true;

    Q_D(QModbusTcpServer);
    if (d->m_tcpServer->isListening())
        return false;

    const QUrl url = QUrl::fromUserInput(d->m_networkAddress + QStringLiteral(":")
        + QString::number(d->m_networkPort));

    if (!url.isValid()) {
        setError(tr("Invalid connection settings for TCP communication specified."),
            QModbusDevice::ConnectionError);
        qCWarning(QT_MODBUS) << "(TCP server) Invalid host:" << url.host() << "or port:"
            << url.port();
        return false;
    }

    if (d->m_tcpServer->listen(QHostAddress(url.host()), url.port()))
        setState(QModbusDevice::ConnectedState);
    else
        setError(d->m_tcpServer->errorString(), QModbusDevice::ConnectionError);

    return state() == QModbusDevice::ConnectedState;
}

/*!
    \reimp
*/
void QModbusTcpServer::close()
{
    if (state() == QModbusDevice::UnconnectedState)
        return;

    Q_D(QModbusTcpServer);

    if (d->m_tcpServer->isListening())
        d->m_tcpServer->close();

    foreach (auto socket, d->connections)
        socket->disconnectFromHost();

    setState(QModbusDevice::UnconnectedState);
}

/*!
    \reimp

    Processes the Modbus client request specified by \a request and returns a
    Modbus response.

    The following Modbus function codes are filtered out as they are serial
    line only according to the Modbus Application Protocol Specification 1.1b:
    \list
        \li \l QModbusRequest::ReadExceptionStatus
        \li \l QModbusRequest::Diagnostics
        \li \l QModbusRequest::GetCommEventCounter
        \li \l QModbusRequest::GetCommEventLog
        \li \l QModbusRequest::ReportServerId
    \endlist
    A request to the TCP server will be answered with a Modbus exception
    response with the exception code QModbusExceptionResponse::IllegalFunction.
*/
QModbusResponse QModbusTcpServer::processRequest(const QModbusPdu &request)
{
    switch (request.functionCode()) {
    case QModbusRequest::ReadExceptionStatus:
    case QModbusRequest::Diagnostics:
    case QModbusRequest::GetCommEventCounter:
    case QModbusRequest::GetCommEventLog:
    case QModbusRequest::ReportServerId:
        return QModbusExceptionResponse(request.functionCode(),
            QModbusExceptionResponse::IllegalFunction);
    default:
        break;
    }
    return QModbusServer::processRequest(request);
}

QT_END_NAMESPACE
