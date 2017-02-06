/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothserver.h"
#include "qbluetoothserver_p.h"
#include "qbluetoothsocket.h"
#ifndef QT_IOS_BLUETOOTH
#include "dummy/dummy_helper_p.h"
#endif

QT_BEGIN_NAMESPACE

QBluetoothServerPrivate::QBluetoothServerPrivate(QBluetoothServiceInfo::Protocol sType)
    : maxPendingConnections(1), serverType(sType), m_lastError(QBluetoothServer::NoError)
{
#ifndef QT_IOS_BLUETOOTH
    printDummyWarning();
#endif
    if (sType == QBluetoothServiceInfo::RfcommProtocol)
        socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    else
        socket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol);
}

QBluetoothServerPrivate::~QBluetoothServerPrivate()
{
    delete socket;
}

void QBluetoothServer::close()
{
}

bool QBluetoothServer::listen(const QBluetoothAddress &address, quint16 port)
{
    Q_UNUSED(address);
    Q_UNUSED(port);
    Q_D(QBluetoothServer);
    d->m_lastError = UnsupportedProtocolError;
    emit error(d->m_lastError);
    return false;
}

void QBluetoothServer::setMaxPendingConnections(int numConnections)
{
    Q_UNUSED(numConnections);
}

bool QBluetoothServer::hasPendingConnections() const
{
    return false;
}

QBluetoothSocket *QBluetoothServer::nextPendingConnection()
{
    return 0;
}

QBluetoothAddress QBluetoothServer::serverAddress() const
{
    return QBluetoothAddress();
}

quint16 QBluetoothServer::serverPort() const
{
    return 0;
}

void QBluetoothServer::setSecurityFlags(QBluetooth::SecurityFlags security)
{
    Q_UNUSED(security);
}

QBluetooth::SecurityFlags QBluetoothServer::securityFlags() const
{
    return QBluetooth::NoSecurity;
}

QT_END_NAMESPACE
