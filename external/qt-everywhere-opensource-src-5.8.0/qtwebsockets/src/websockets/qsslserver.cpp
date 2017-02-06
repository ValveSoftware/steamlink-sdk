/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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

/*!
    \class QSslServer

    \inmodule QtWebSockets

    \brief Implements a secure TCP server over SSL.

    \internal
*/

#include "qsslserver_p.h"

#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslCipher>

QT_BEGIN_NAMESPACE

/*!
    Constructs a new QSslServer with the given \a parent.

    \internal
*/
QSslServer::QSslServer(QObject *parent) :
    QTcpServer(parent),
    m_sslConfiguration(QSslConfiguration::defaultConfiguration())
{
}

/*!
    Destroys the QSslServer.

    All open connections are closed.

    \internal
*/
QSslServer::~QSslServer()
{
}

/*!
    Sets the \a sslConfiguration to use.

    \sa QSslSocket::setSslConfiguration()

    \internal
*/
void QSslServer::setSslConfiguration(const QSslConfiguration &sslConfiguration)
{
    m_sslConfiguration = sslConfiguration;
}

/*!
    Returns the current ssl configuration.

    \internal
*/
QSslConfiguration QSslServer::sslConfiguration() const
{
    return m_sslConfiguration;
}

/*!
    Called when a new connection is established.

    Converts \a socket to a QSslSocket.

    \internal
*/
void QSslServer::incomingConnection(qintptr socket)
{
    QSslSocket *pSslSocket = new QSslSocket();

    if (Q_LIKELY(pSslSocket)) {
        pSslSocket->setSslConfiguration(m_sslConfiguration);

        if (Q_LIKELY(pSslSocket->setSocketDescriptor(socket))) {
            connect(pSslSocket, &QSslSocket::peerVerifyError, this, &QSslServer::peerVerifyError);

            typedef void (QSslSocket::* sslErrorsSignal)(const QList<QSslError> &);
            connect(pSslSocket, static_cast<sslErrorsSignal>(&QSslSocket::sslErrors),
                    this, &QSslServer::sslErrors);
            connect(pSslSocket, &QSslSocket::encrypted, this, &QSslServer::newEncryptedConnection);
            connect(pSslSocket, &QSslSocket::preSharedKeyAuthenticationRequired, this, &QSslServer::preSharedKeyAuthenticationRequired);

            addPendingConnection(pSslSocket);

            pSslSocket->startServerEncryption();
        } else {
           delete pSslSocket;
        }
    }
}

QT_END_NAMESPACE
