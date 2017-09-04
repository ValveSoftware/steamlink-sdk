/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
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

#include "qconnection_tcpip_backend_p.h"

#include <QHostInfo>

QT_BEGIN_NAMESPACE

TcpClientIo::TcpClientIo(QObject *parent)
    : ClientIoDevice(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead, this, &ClientIoDevice::readyRead);
    connect(&m_socket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this, &TcpClientIo::onError);
    connect(&m_socket, &QTcpSocket::stateChanged, this, &TcpClientIo::onStateChanged);
}

TcpClientIo::~TcpClientIo()
{
    close();
}

QIODevice *TcpClientIo::connection()
{
    return &m_socket;
}

void TcpClientIo::doClose()
{
    if (m_socket.isOpen()) {
        connect(&m_socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
        m_socket.disconnectFromHost();
    } else {
        this->deleteLater();
    }
}

void TcpClientIo::connectToServer()
{
    if (isOpen())
        return;
    QHostAddress address(url().host());
    if (address.isNull()) {
        const QList<QHostAddress> addresses = QHostInfo::fromName(url().host()).addresses();
        Q_ASSERT_X(addresses.size() >= 1, Q_FUNC_INFO, url().toString().toLatin1().data());
        address = addresses.first();
    }

    m_socket.connectToHost(address, url().port());
}

bool TcpClientIo::isOpen()
{
    return (!isClosing() && (m_socket.state() == QAbstractSocket::ConnectedState
                             || m_socket.state() == QAbstractSocket::ConnectingState));
}

void TcpClientIo::onError(QAbstractSocket::SocketError error)
{
    qCDebug(QT_REMOTEOBJECT) << "onError" << error;

    switch (error) {
    case QAbstractSocket::HostNotFoundError:     //Host not there, wait and try again
    case QAbstractSocket::ConnectionRefusedError:
        emit shouldReconnect(this);
        break;
    case QAbstractSocket::AddressInUseError:
        //... TODO error reporting
        break;
    default:
        break;
    }
}

void TcpClientIo::onStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::ClosingState && !isClosing()) {
        m_socket.abort();
        emit shouldReconnect(this);
    }
    if (state == QAbstractSocket::ConnectedState) {
        m_dataStream.setDevice(connection());
        m_dataStream.resetStatus();
    }
}


TcpServerIo::TcpServerIo(QTcpSocket *conn, QObject *parent)
    : ServerIoDevice(parent), m_connection(conn)
{
    m_connection->setParent(this);
    connect(conn, &QIODevice::readyRead, this, &ServerIoDevice::readyRead);
    connect(conn, &QAbstractSocket::disconnected, this, &ServerIoDevice::disconnected);
}

QIODevice *TcpServerIo::connection() const
{
    return m_connection;
}

void TcpServerIo::doClose()
{
    m_connection->disconnectFromHost();
}



TcpServerImpl::TcpServerImpl(QObject *parent)
    : QConnectionAbstractServer(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &QConnectionAbstractServer::newConnection);
}

TcpServerImpl::~TcpServerImpl()
{
    close();
}

ServerIoDevice *TcpServerImpl::configureNewConnection()
{
    if (!m_server.isListening())
        return nullptr;

    return new TcpServerIo(m_server.nextPendingConnection());
}

bool TcpServerImpl::hasPendingConnections() const
{
    return m_server.hasPendingConnections();
}

QUrl TcpServerImpl::address() const
{
    return m_originalUrl;
}

bool TcpServerImpl::listen(const QUrl &address)
{
    QHostAddress host(address.host());
    if (host.isNull()) {
        if (address.host().isEmpty()) {
            host = QHostAddress::Any;
        } else {
            qCWarning(QT_REMOTEOBJECT) << address.host() << " is not an IP address, trying to resolve it";
            QHostInfo info = QHostInfo::fromName(address.host());
            if (info.addresses().isEmpty())
                host = QHostAddress::Any;
            else
                host = info.addresses().takeFirst();
        }
    }

    bool ret = m_server.listen(host, address.port());
    if (ret) {
        m_originalUrl.setScheme(QLatin1String("tcp"));
        m_originalUrl.setHost(m_server.serverAddress().toString());
        m_originalUrl.setPort(m_server.serverPort());
    }
    return ret;
}

QAbstractSocket::SocketError TcpServerImpl::serverError() const
{
    return m_server.serverError();
}

void TcpServerImpl::close()
{
    m_server.close();
}

QT_END_NAMESPACE
