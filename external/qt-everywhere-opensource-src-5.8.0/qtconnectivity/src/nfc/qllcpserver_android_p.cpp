/****************************************************************************
**
** Copyright (C) 2016 Centria research and development
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qllcpserver_android_p.h"
//#include "qnx/qnxnfcmanager_p.h"

QT_BEGIN_NAMESPACE

QLlcpServerPrivate::QLlcpServerPrivate(QLlcpServer *q)
    : q_ptr(q), m_llcpSocket(0), m_connected(false)
{
}

QLlcpServerPrivate::~QLlcpServerPrivate()
{
}

bool QLlcpServerPrivate::listen(const QString &/*serviceUri*/)
{
    /*//The server is already listening
    if (isListening())
        return false;

    nfc_result_t result = nfc_llcp_register_connection_listener(NFC_LLCP_SERVER, 0, serviceUri.toStdString().c_str(), &m_conListener);
    m_connected = true;
    if (result == NFC_RESULT_SUCCESS) {
        m_serviceUri = serviceUri;
        qQNXNFCDebug() << "LLCP server registered" << serviceUri;
    } else {
        qWarning() << Q_FUNC_INFO << "Could not register for llcp connection listener";
        return false;
    }
    QNXNFCManager::instance()->registerLLCPConnection(m_conListener, this);*/
    return true;
}

bool QLlcpServerPrivate::isListening() const
{
    return m_connected;
}

void QLlcpServerPrivate::close()
{
    /*nfc_llcp_unregister_connection_listener(m_conListener);
    QNXNFCManager::instance()->unregisterLLCPConnection(m_conListener);
    m_serviceUri = QString();
    m_connected = false;*/
}

QString QLlcpServerPrivate::serviceUri() const
{
    return m_serviceUri;
}

quint8 QLlcpServerPrivate::serverPort() const
{
    /*unsigned int sap;
    if (nfc_llcp_get_local_sap(m_target, &sap) == NFC_RESULT_SUCCESS) {
        return sap;
    }*/
    return -1;
}

bool QLlcpServerPrivate::hasPendingConnections() const
{
    return m_llcpSocket != 0;
}

QLlcpSocket *QLlcpServerPrivate::nextPendingConnection()
{
    /*QLlcpSocket *socket = m_llcpSocket;
    m_llcpSocket = 0;
    return socket;*/
    return 0;
}

QLlcpSocket::SocketError QLlcpServerPrivate::serverError() const
{
    return QLlcpSocket::UnknownSocketError;
}

/*void QLlcpServerPrivate::connected(nfc_target_t *target)
{
    m_target = target;
    if (m_llcpSocket != 0) {
        qWarning() << Q_FUNC_INFO << "LLCP socket not cloesed properly";
        return;
    }
    m_llcpSocket = new QLlcpSocket();
    m_llcpSocket->bind(serverPort());
}*/

QT_END_NAMESPACE


