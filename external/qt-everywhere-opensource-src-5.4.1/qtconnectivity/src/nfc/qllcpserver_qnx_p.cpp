/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qllcpserver_qnx_p.h"
#include "qnx/qnxnfcmanager_p.h"

QT_BEGIN_NAMESPACE

QLlcpServerPrivate::QLlcpServerPrivate(QLlcpServer *q)
    : q_ptr(q), m_llcpSocket(0), m_connected(false), m_conListener(0)
{
}

bool QLlcpServerPrivate::listen(const QString &serviceUri)
{
    //The server is already listening
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
    QNXNFCManager::instance()->registerLLCPConnection(m_conListener, this);
    return true;
}

bool QLlcpServerPrivate::isListening() const
{
    return m_connected;
}

void QLlcpServerPrivate::close()
{
    nfc_llcp_unregister_connection_listener(m_conListener);
    QNXNFCManager::instance()->unregisterLLCPConnection(m_conListener);
    m_serviceUri = QString();
    m_connected = false;
}

QString QLlcpServerPrivate::serviceUri() const
{
    return m_serviceUri;
}

quint8 QLlcpServerPrivate::serverPort() const
{
    unsigned int sap;
    if (nfc_llcp_get_local_sap(m_target, &sap) == NFC_RESULT_SUCCESS) {
        return sap;
    }
    return -1;
}

bool QLlcpServerPrivate::hasPendingConnections() const
{
    return m_llcpSocket != 0;
}

QLlcpSocket *QLlcpServerPrivate::nextPendingConnection()
{
    QLlcpSocket *socket = m_llcpSocket;
    m_llcpSocket = 0;
    return socket;
}

QLlcpSocket::SocketError QLlcpServerPrivate::serverError() const
{
    return QLlcpSocket::UnknownSocketError;
}

void QLlcpServerPrivate::connected(nfc_target_t *target)
{
    m_target = target;
    if (m_llcpSocket != 0) {
        qWarning() << Q_FUNC_INFO << "LLCP socket not cloesed properly";
        return;
    }
    m_llcpSocket = new QLlcpSocket();
    m_llcpSocket->bind(serverPort());
}

QT_END_NAMESPACE


