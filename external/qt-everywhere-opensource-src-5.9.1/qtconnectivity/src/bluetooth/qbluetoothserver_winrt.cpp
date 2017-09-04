/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "qbluetoothsocket_p.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/private/qeventdispatcher_winrt_p.h>
#include <qfunctions_winrt.h>

#include <windows.networking.h>
#include <windows.networking.connectivity.h>
#include <windows.networking.sockets.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Networking;
using namespace ABI::Windows::Networking::Sockets;
using namespace ABI::Windows::Networking::Connectivity;

typedef ITypedEventHandler<StreamSocketListener *, StreamSocketListenerConnectionReceivedEventArgs *> ClientConnectedHandler;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_WINRT)

QHash<QBluetoothServerPrivate *, int> __fakeServerPorts;

QBluetoothServerPrivate::QBluetoothServerPrivate(QBluetoothServiceInfo::Protocol sType)
    : maxPendingConnections(1), serverType(sType), m_lastError(QBluetoothServer::NoError), socket(0)
{
    socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
}

QBluetoothServerPrivate::~QBluetoothServerPrivate()
{
    deactivateActiveListening();
    __fakeServerPorts.remove(this);
    if (socket)
        delete socket;
}

bool QBluetoothServerPrivate::isListening() const
{
    return __fakeServerPorts.contains(const_cast<QBluetoothServerPrivate *>(this));
}

bool QBluetoothServerPrivate::initiateActiveListening(const QString &serviceName)
{
    HStringReference serviceNameRef(reinterpret_cast<LPCWSTR>(serviceName.utf16()));

    ComPtr<IAsyncAction> bindAction;
    HRESULT hr = socketListener->BindServiceNameAsync(serviceNameRef.Get(), &bindAction);
    Q_ASSERT_SUCCEEDED(hr);
    hr = QWinRTFunctions::await(bindAction);
    Q_ASSERT_SUCCEEDED(hr);
    return true;
}

bool QBluetoothServerPrivate::deactivateActiveListening()
{
    if (!isListening())
        return true;

    HRESULT hr;
    hr = socketListener->remove_ConnectionReceived(connectionToken);
    Q_ASSERT_SUCCEEDED(hr);
    return true;
}

HRESULT QBluetoothServerPrivate::handleClientConnection(IStreamSocketListener *listener,
                                                        IStreamSocketListenerConnectionReceivedEventArgs *args)
{
    Q_Q(QBluetoothServer);
    if (!socketListener || socketListener.Get() != listener) {
        qCDebug(QT_BT_WINRT) << "Accepting connection from wrong listener. We should not be here.";
        Q_UNREACHABLE();
        return S_OK;
    }

    HRESULT hr;
    ComPtr<IStreamSocket> socket;
    hr = args->get_Socket(&socket);
    Q_ASSERT_SUCCEEDED(hr);
    QMutexLocker locker(&pendingConnectionsMutex);
    if (pendingConnections.count() < maxPendingConnections) {
        qCDebug(QT_BT_WINRT) << "Accepting connection";
        pendingConnections.append(socket);
        q->newConnection();
    } else {
        qCDebug(QT_BT_WINRT) << "Refusing connection";
    }

    return S_OK;
}

void QBluetoothServer::close()
{
    Q_D(QBluetoothServer);

    d->deactivateActiveListening();
    __fakeServerPorts.remove(d);
}

bool QBluetoothServer::listen(const QBluetoothAddress &address, quint16 port)
{
    Q_UNUSED(address);
    Q_D(QBluetoothServer);
    if (serverType() != QBluetoothServiceInfo::RfcommProtocol) {
        d->m_lastError = UnsupportedProtocolError;
        emit error(d->m_lastError);
        return false;
    }

    if (isListening())
        return false;

    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d, this] ()
    {
        HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Networking_Sockets_StreamSocketListener).Get(),
                                &d->socketListener);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->socketListener->add_ConnectionReceived(Callback<ClientConnectedHandler>(d, &QBluetoothServerPrivate::handleClientConnection).Get(),
                                                       &d->connectionToken);
        Q_ASSERT_SUCCEEDED(hr);
        return S_OK;
    });
    Q_ASSERT_SUCCEEDED(hr);

    //We can not register an actual Rfcomm port, because the platform does not allow it
    //but we need a way to associate a server with a service
    if (port == 0) { //Try to assign a non taken port id
        for (int i = 1; ; i++){
            if (__fakeServerPorts.key(i) == 0) {
                port = i;
                break;
            }
        }
    }

    if (__fakeServerPorts.key(port) == 0) {
        __fakeServerPorts[d] = port;

        qCDebug(QT_BT_WINRT) << "Port" << port << "registered";
    } else {
        qCWarning(QT_BT_WINRT) << "server with port" << port << "already registered or port invalid";
        d->m_lastError = ServiceAlreadyRegisteredError;
        emit error(d->m_lastError);
        return false;
    }

    return true;
}

void QBluetoothServer::setMaxPendingConnections(int numConnections)
{
    Q_D(QBluetoothServer);
    QMutexLocker locker(&d->pendingConnectionsMutex);
    if (d->pendingConnections.count() > numConnections) {
        qCWarning(QT_BT_WINRT) << "There are currently more than" << numConnections << "connections"
                               << "pending. Number of maximum pending connections was not changed.";
        return;
    }

    d->maxPendingConnections = numConnections;
}

bool QBluetoothServer::hasPendingConnections() const
{
    Q_D(const QBluetoothServer);
    QMutexLocker locker(&d->pendingConnectionsMutex);
    return !d->pendingConnections.isEmpty();
}

QBluetoothSocket *QBluetoothServer::nextPendingConnection()
{
    Q_D(QBluetoothServer);

    ComPtr<IStreamSocket> socket = d->pendingConnections.takeFirst();

    QBluetoothSocket *newSocket = new QBluetoothSocket();
    bool success = newSocket->d_ptr->setSocketDescriptor(socket, d->serverType);
    if (!success) {
        delete newSocket;
        newSocket = 0;
    }

    return newSocket;
}

QBluetoothAddress QBluetoothServer::serverAddress() const
{
    return QBluetoothAddress();
}

quint16 QBluetoothServer::serverPort() const
{
    //We return the fake port
    Q_D(const QBluetoothServer);
    return __fakeServerPorts.value((QBluetoothServerPrivate*)d, 0);
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
