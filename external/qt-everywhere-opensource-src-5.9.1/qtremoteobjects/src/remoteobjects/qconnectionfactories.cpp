/****************************************************************************
**
** Copyright (C) 2017-2015 Ford Motor Company
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

#include "qconnectionfactories.h"
#include "qconnectionfactories_p.h"

// BEGIN: Backends
#if defined(Q_OS_QNX)
#include "qconnection_qnx_backend_p.h"
#endif
#include "qconnection_local_backend_p.h"
#include "qconnection_tcpip_backend_p.h"
// END: Backends

QT_BEGIN_NAMESPACE

using namespace QtRemoteObjects;

class QtROFactoryLoader
{
public:
    QtROClientFactory clientFactory;
    QtROServerFactory serverFactory;
};

Q_GLOBAL_STATIC(QtROFactoryLoader, loader)

inline bool fromDataStream(QDataStream &in, QRemoteObjectPacketTypeEnum &type, QString &name)
{
    quint16 _type;
    in >> _type;
    type = Invalid;
    switch (_type) {
    case InitPacket: type = InitPacket; break;
    case InitDynamicPacket: type = InitDynamicPacket; break;
    case AddObject: type = AddObject; break;
    case RemoveObject: type = RemoveObject; break;
    case InvokePacket: type = InvokePacket; break;
    case InvokeReplyPacket: type = InvokeReplyPacket; break;
    case PropertyChangePacket: type = PropertyChangePacket; break;
    case ObjectList: type = ObjectList; break;
    default:
        qCWarning(QT_REMOTEOBJECT_IO) << "Invalid packet received" << type;
    }
    if (type == Invalid)
        return false;
    if (type == ObjectList)
        return true;
    in >> name;
    return true;
}

ClientIoDevice::ClientIoDevice(QObject *parent)
    : QObject(parent), m_isClosing(false), m_curReadSize(0)
{
    m_dataStream.setVersion(dataStreamVersion);
}

ClientIoDevice::~ClientIoDevice()
{
    if (!m_isClosing)
        close();
}

void ClientIoDevice::close()
{
    m_isClosing = true;
    doClose();
}

bool ClientIoDevice::read(QRemoteObjectPacketTypeEnum &type, QString &name)
{
    qCDebug(QT_REMOTEOBJECT_IO) << "ClientIODevice::read()" << m_curReadSize << bytesAvailable();

    if (m_curReadSize == 0) {
        if (bytesAvailable() < static_cast<int>(sizeof(quint32)))
            return false;

        m_dataStream >> m_curReadSize;
    }

    qCDebug(QT_REMOTEOBJECT_IO) << "ClientIODevice::read()-looking for map" << m_curReadSize << bytesAvailable();

    if (bytesAvailable() < m_curReadSize)
        return false;

    m_curReadSize = 0;
    return fromDataStream(m_dataStream, type, name);
}

void ClientIoDevice::write(const QByteArray &data)
{
    connection()->write(data);
}

void ClientIoDevice::write(const QByteArray &data, qint64 size)
{
    connection()->write(data.data(), size);
}

qint64 ClientIoDevice::bytesAvailable()
{
    return connection()->bytesAvailable();
}

QUrl ClientIoDevice::url() const
{
    return m_url;
}

void ClientIoDevice::addSource(const QString &name)
{
    m_remoteObjects.insert(name);
}

void ClientIoDevice::removeSource(const QString &name)
{
    m_remoteObjects.remove(name);
}

QSet<QString> ClientIoDevice::remoteObjects() const
{
    return m_remoteObjects;
}

ServerIoDevice::ServerIoDevice(QObject *parent)
    : QObject(parent), m_isClosing(false), m_curReadSize(0)
{
    m_dataStream.setVersion(dataStreamVersion);
}

ServerIoDevice::~ServerIoDevice()
{
}

bool ServerIoDevice::read(QRemoteObjectPacketTypeEnum &type, QString &name)
{
    qCDebug(QT_REMOTEOBJECT_IO) << "ServerIODevice::read()" << m_curReadSize << bytesAvailable();

    if (m_curReadSize == 0) {
        if (bytesAvailable() < static_cast<int>(sizeof(quint32)))
            return false;

        m_dataStream >> m_curReadSize;
    }

    qCDebug(QT_REMOTEOBJECT_IO) << "ServerIODevice::read()-looking for map" << m_curReadSize << bytesAvailable();

    if (bytesAvailable() < m_curReadSize)
        return false;

    m_curReadSize = 0;
    return fromDataStream(m_dataStream, type, name);
}

void ServerIoDevice::close()
{
    m_isClosing = true;
    doClose();
}

void ServerIoDevice::write(const QByteArray &data)
{
    if (connection()->isOpen() && !m_isClosing)
        connection()->write(data);
}

void ServerIoDevice::write(const QByteArray &data, qint64 size)
{
    if (connection()->isOpen() && !m_isClosing)
        connection()->write(data.data(), size);
}

qint64 ServerIoDevice::bytesAvailable()
{
    return connection()->bytesAvailable();
}

void ServerIoDevice::initializeDataStream()
{
    m_dataStream.setDevice(connection());
    m_dataStream.resetStatus();
}

QConnectionAbstractServer::QConnectionAbstractServer(QObject *parent)
    : QObject(parent)
{
}

QConnectionAbstractServer::~QConnectionAbstractServer()
{
}

ServerIoDevice *QConnectionAbstractServer::nextPendingConnection()
{
    ServerIoDevice *iodevice = configureNewConnection();
    iodevice->initializeDataStream();
    return iodevice;
}

/*!
    \class QtROServerFactory
    \inmodule QtRemoteObjects
    \brief A class holding information about server backends available on the Qt Remote Objects network
*/
QtROServerFactory::QtROServerFactory()
{
#if defined(Q_OS_QNX)
    registerType<QnxServerImpl>(QStringLiteral("qnx"));
#endif
    registerType<LocalServerImpl>(QStringLiteral("local"));
    registerType<TcpServerImpl>(QStringLiteral("tcp"));
}

QtROServerFactory *QtROServerFactory::instance()
{
    return &loader->serverFactory;
}

/*!
    \class QtROClientFactory
    \inmodule QtRemoteObjects
    \brief A class holding information about client backends available on the Qt Remote Objects network
*/
QtROClientFactory::QtROClientFactory()
{
#if defined(Q_OS_QNX)
    registerType<QnxClientIo>(QStringLiteral("qnx"));
#endif
    registerType<LocalClientIo>(QStringLiteral("local"));
    registerType<TcpClientIo>(QStringLiteral("tcp"));
}

QtROClientFactory *QtROClientFactory::instance()
{
    return &loader->clientFactory;
}

/*!
    \fn void qRegisterRemoteObjectsClient(const QString &id)
    \relates QtROClientFactory

    Registers the Remote Objects client \a id for the type \c{T}.

    If you need a custom transport protocol for Qt Remote Objects, you need to
    register the client & server implementation here.

    \note This function requires that \c{T} is a fully defined type at the point
    where the function is called.

    This example registers the class \c{CustomClientIo} as \c{"myprotocol"}:

    \code
        qRegisterRemoteObjectsClient<CustomClientIo>(QStringLiteral("myprotocol"));
    \endcode

    With this in place, you can now instantiate nodes using this new custom protocol:

    \code
        QRemoteObjectNode client(QUrl(QStringLiteral("myprotocol:registry")));
    \endcode

    \sa {qRegisterRemoteObjectsServer}
*/

/*!
    \fn void qRegisterRemoteObjectsServer(const QString &id)
    \relates QtROServerFactory

    Registers the Remote Objects server \a id for the type \c{T}.

    If you need a custom transport protocol for Qt Remote Objects, you need to
    register the client & server implementation here.

    \note This function requires that \c{T} is a fully defined type at the point
    where the function is called.

    This example registers the class \c{CustomServerImpl} as \c{"myprotocol"}:

    \code
        qRegisterRemoteObjectsServer<CustomServerImpl>(QStringLiteral("myprotocol"));
    \endcode

    With this in place, you can now instantiate nodes using this new custom protocol:

    \code
        QRemoteObjectNode client(QUrl(QStringLiteral("myprotocol:registry")));
    \endcode

    \sa {qRegisterRemoteObjectsServer}
*/

QT_END_NAMESPACE
