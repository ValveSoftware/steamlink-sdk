/****************************************************************************
**
** Copyright (C) 2017-2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "qconnection_qnx_backend_p.h"

QT_BEGIN_NAMESPACE

QnxClientIo::QnxClientIo(QObject *parent)
    : ClientIoDevice(parent)
{
    connect(&m_socket, &QQnxNativeIo::readyRead, this, &ClientIoDevice::readyRead);
    connect(&m_socket,
            static_cast<void(QQnxNativeIo::*)(QAbstractSocket::SocketError)>(&QQnxNativeIo::error),
            this,
            &QnxClientIo::onError);
    connect(&m_socket, &QQnxNativeIo::stateChanged, this, &QnxClientIo::onStateChanged);
}

QnxClientIo::~QnxClientIo()
{
    close();
}

QIODevice *QnxClientIo::connection()
{
    return &m_socket;
}

void QnxClientIo::doClose()
{
    if (m_socket.isOpen()) {
        connect(&m_socket, &QQnxNativeIo::disconnected, this, &QObject::deleteLater);
        m_socket.disconnectFromServer();
    } else {
        deleteLater();
    }
}

void QnxClientIo::connectToServer()
{
    if (!isOpen())
        m_socket.connectToServer(url().path());
}

bool QnxClientIo::isOpen()
{
    return !isClosing() && m_socket.isOpen();
}

void QnxClientIo::onError(QAbstractSocket::SocketError error)
{
    qCDebug(QT_REMOTEOBJECT) << "onError" << error << m_socket.serverName();

    switch (error) {
    case QAbstractSocket::RemoteHostClosedError:
        m_socket.close();
        qCWarning(QT_REMOTEOBJECT) << "RemoteHostClosedError";
    case QAbstractSocket::HostNotFoundError:     //Host not there, wait and try again
    case QAbstractSocket::AddressInUseError:
    case QAbstractSocket::ConnectionRefusedError:
        //... TODO error reporting
        emit shouldReconnect(this);
        break;
    default:
        break;
    }
}

void QnxClientIo::onStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ClosingState && !isClosing()) {
        m_socket.abort();
        emit shouldReconnect(this);
    } else if (state == QAbstractSocket::ConnectedState) {
        m_dataStream.setDevice(connection());
        m_dataStream.resetStatus();
    }
}

QnxServerIo::QnxServerIo(QIOQnxSource *conn, QObject *parent)
    : ServerIoDevice(parent), m_connection(conn)
{
    m_connection->setParent(this);
    connect(conn, &QIODevice::readyRead, this, &ServerIoDevice::readyRead);
    connect(conn, &QIOQnxSource::disconnected, this, &ServerIoDevice::disconnected);
}

QIODevice *QnxServerIo::connection() const
{
    return m_connection;
}

void QnxServerIo::doClose()
{
    m_connection->close();
}

QnxServerImpl::QnxServerImpl(QObject *parent)
    : QConnectionAbstractServer(parent)
{
    connect(&m_server,
            &QQnxNativeServer::newConnection,
            this,
            &QConnectionAbstractServer::newConnection);
}

QnxServerImpl::~QnxServerImpl()
{
    m_server.close();
}

ServerIoDevice *QnxServerImpl::configureNewConnection()
{
    if (!m_server.isListening())
        return nullptr;

    return new QnxServerIo(m_server.nextPendingConnection(), this);
}

bool QnxServerImpl::hasPendingConnections() const
{
    return m_server.hasPendingConnections();
}

QUrl QnxServerImpl::address() const
{
    QUrl result;
    result.setPath(m_server.serverName());
    result.setScheme(QStringLiteral("qnx"));

    return result;
}

bool QnxServerImpl::listen(const QUrl &address)
{
    return m_server.listen(address.path());
}

QAbstractSocket::SocketError QnxServerImpl::serverError() const
{
    //TODO implement on QQnxNativeServer and here
    //return m_server.serverError();
    return QAbstractSocket::AddressInUseError;
}

void QnxServerImpl::close()
{
    close();
}

QT_END_NAMESPACE
