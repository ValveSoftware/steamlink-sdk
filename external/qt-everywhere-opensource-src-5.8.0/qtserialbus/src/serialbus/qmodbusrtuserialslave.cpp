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

#include "qmodbusrtuserialslave.h"
#include "qmodbusrtuserialslave_p.h"

#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

/*!
    \class QModbusRtuSerialSlave
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusRtuSerialSlave class represents a Modbus server
    that uses a serial port for its communication with the Modbus client.

    Communication via Modbus requires the interaction between a single Modbus
    client instance and multiple Modbus server. This class provides the Modbus
    server implementation via a serial port.

    Since multiple Modbus server instances can interact with a Modbus client
    at the same time (using a serial bus), servers are identified by their
    \l serverAddress().
*/

/*!
    Constructs a QModbusRtuSerialSlave with the specified \a parent. The
    \l serverAddress preset is \c 1.
*/
QModbusRtuSerialSlave::QModbusRtuSerialSlave(QObject *parent)
    : QModbusServer(*new QModbusRtuSerialSlavePrivate, parent)
{
    Q_D(QModbusRtuSerialSlave);
    d->setupSerialPort();
}

/*!
    Destroys the QModbusRtuSerialSlave instance.
*/
QModbusRtuSerialSlave::~QModbusRtuSerialSlave()
{
    close();
}

/*!
    \internal
*/
QModbusRtuSerialSlave::QModbusRtuSerialSlave(QModbusRtuSerialSlavePrivate &dd, QObject *parent)
    : QModbusServer(dd, parent)
{
    Q_D(QModbusRtuSerialSlave);
    d->setupSerialPort();
}

/*!
    \reimp
*/
bool QModbusRtuSerialSlave::processesBroadcast() const
{
    return d_func()->m_processesBroadcast;
}

/*!
    \reimp

     \note When calling this function, existing buffered data is removed from
     the serial port.
*/
bool QModbusRtuSerialSlave::open()
{
    if (state() == QModbusDevice::ConnectedState)
        return true;

    Q_D(QModbusRtuSerialSlave);
    d->setupEnvironment(); // to be done before open
    if (d->m_serialPort->open(QIODevice::ReadWrite)) {
        setState(QModbusDevice::ConnectedState);
        d->m_serialPort->clear(); // only possible after open
    } else {
        setError(d->m_serialPort->errorString(), QModbusDevice::ConnectionError);
    }
    return (state() == QModbusDevice::ConnectedState);
}

/*!
    \reimp
*/
void QModbusRtuSerialSlave::close()
{
    if (state() == QModbusDevice::UnconnectedState)
        return;

    Q_D(QModbusRtuSerialSlave);
    if (d->m_serialPort->isOpen())
        d->m_serialPort->close();

    setState(QModbusDevice::UnconnectedState);
}

/*!
    \reimp

    Processes the Modbus client request specified by \a request and returns a
    Modbus response.

    The Modbus function \l QModbusRequest::EncapsulatedInterfaceTransport with
    MEI Type 13 (0x0D) CANopen General Reference is filtered out because it is
    usually Modbus TCP or Modbus serial ASCII only.

    A request to the RTU serial slave will be answered with a Modbus exception
    response with the exception code QModbusExceptionResponse::IllegalFunction.
*/
QModbusResponse QModbusRtuSerialSlave::processRequest(const QModbusPdu &request)
{
    if (request.functionCode() == QModbusRequest::EncapsulatedInterfaceTransport) {
        quint8 meiType;
        request.decodeData(&meiType);
        if (meiType == EncapsulatedInterfaceTransport::CanOpenGeneralReference) {
            return QModbusExceptionResponse(request.functionCode(),
                QModbusExceptionResponse::IllegalFunction);
        }
    }
    return QModbusServer::processRequest(request);
}

#include "moc_qmodbusrtuserialslave.cpp"

QT_END_NAMESPACE
