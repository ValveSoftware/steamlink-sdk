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

#include "qdeclarativebluetoothservice_p.h"

#include <QtCore/QLoggingCategory>

#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothSocket>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothServer>

#include <QObject>

/* ==================== QDeclarativeBluetoothService ======================= */

/*!
    \qmltype BluetoothService
    \instantiates QDeclarativeBluetoothService
    \inqmlmodule QtBluetooth
    \since 5.2
    \brief Provides information about a particular Bluetooth service.

    \sa QBluetoothAddress
    \sa QBluetoothSocket

    It allows a QML project to get information about a remote service, or describe a service
    for a BluetoothSocket to connect to.
 */

/*!
    \qmlsignal BluetoothService::detailsChanged()

    This signal is emitted when any of the following properties changes:

    \list
        \li deviceAddress
        \li deviceName
        \li serviceDescription
        \li serviceName
        \li serviceProtocol
        \li serviceUuid
    \endlist

    The corresponding handler is \c onDetailsChanged.
*/

Q_DECLARE_LOGGING_CATEGORY(QT_BT_QML)

class QDeclarativeBluetoothServicePrivate
{
public:
    QDeclarativeBluetoothServicePrivate()
        : m_componentComplete(false),
          m_service(0),
          m_server(0)
    {

    }

    ~QDeclarativeBluetoothServicePrivate()
    {
        delete m_service;
    }

    bool m_componentComplete;
    QBluetoothServiceInfo *m_service;
    QDeclarativeBluetoothService::Protocol m_protocol;
    QBluetoothServer *m_server;
};

QDeclarativeBluetoothService::QDeclarativeBluetoothService(QObject *parent) :
    QObject(parent)
{
    d = new QDeclarativeBluetoothServicePrivate;
    d->m_service = new QBluetoothServiceInfo();
}

QDeclarativeBluetoothService::QDeclarativeBluetoothService(const QBluetoothServiceInfo &service, QObject *parent)
    : QObject(parent)
{
    d = new QDeclarativeBluetoothServicePrivate;
    d->m_service = new QBluetoothServiceInfo(service);
}

QDeclarativeBluetoothService::~QDeclarativeBluetoothService()
{
    delete d;
}

void QDeclarativeBluetoothService::componentComplete()
{
    d->m_componentComplete = true;

    if (!d->m_service->isRegistered())
        setRegistered(true);
}


/*!
    \qmlproperty string BluetoothService::deviceName

    This property holds the name of the remote device. Changing this property emits the
    detailsChanged signal.
  */

QString QDeclarativeBluetoothService::deviceName() const
{

    return d->m_service->device().name();
}

/*!
    \qmlproperty string BluetoothService::deviceAddress

    This property holds the remote device's MAC address. It must be a valid address to
    connect to a remote device using a Bluetooth socket. Changing this property emits the
    detailsChanged signal.

  */

QString QDeclarativeBluetoothService::deviceAddress() const
{
    return d->m_service->device().address().toString();
}

void QDeclarativeBluetoothService::setDeviceAddress(const QString &newAddress)
{
    QBluetoothAddress address(newAddress);
    QBluetoothDeviceInfo device(address, QString(), QBluetoothDeviceInfo::ComputerDevice);
    d->m_service->setDevice(device);
    emit detailsChanged();
}

/*!
    \qmlproperty string BluetoothService::serviceName

    This property holds the name of the remote service if available. Changing this property emits the
    detailsChanged signal.
  */

QString QDeclarativeBluetoothService::serviceName() const
{
    return d->m_service->serviceName();
}

void QDeclarativeBluetoothService::setServiceName(const QString &name)
{
    d->m_service->setServiceName(name);
    emit detailsChanged();
}


/*!
    \qmlproperty string BluetoothService::serviceDescription

    This property holds the description provided by the remote service. Changing this property emits the
    detailsChanged signal.
  */
QString QDeclarativeBluetoothService::serviceDescription() const
{
    return d->m_service->serviceDescription();
}

void QDeclarativeBluetoothService::setServiceDescription(const QString &description)
{
    d->m_service->setServiceDescription(description);
    emit detailsChanged();
}

/*!
    \qmlproperty enumeration BluetoothService::serviceProtocol

    This property holds the protocol used for the service. Changing this property emits the
    detailsChanged signal.

    Possible values for this property are:
    \table
    \header \li Property \li Description
    \row \li \c BluetoothService.RfcommProtocol
         \li The Rfcomm protocol is used.
    \row \li \c BluetoothService.L2capProtocol
         \li The L2cap protocol is used.
    \row \li \c BluetoothService.UnknownProtocol
         \li The protocol is unknown.
    \endtable

    \sa QBluetoothServiceInfo::Protocol

  */

QDeclarativeBluetoothService::Protocol QDeclarativeBluetoothService::serviceProtocol() const
{
    return d->m_protocol;
}

void QDeclarativeBluetoothService::setServiceProtocol(QDeclarativeBluetoothService::Protocol protocol)
{
    d->m_protocol = protocol;
    emit detailsChanged();
}

/*!
    \qmlproperty string BluetoothService::serviceUuid

    This property holds the UUID of the remote service. Service UUID,
    and the address must be set to connect to a remote service. Changing
    this property emits the detailsChanged signal.

*/

QString QDeclarativeBluetoothService::serviceUuid() const
{
    return d->m_service->serviceUuid().toString();
}

void QDeclarativeBluetoothService::setServiceUuid(const QString &uuid)
{
    d->m_service->setServiceUuid(QBluetoothUuid(uuid));
    emit detailsChanged();
}

/*!
    \qmlproperty string BluetoothService::registered

    This property holds the registration/publication status of the service.  If true, the service
    is published during service discovery.
*/

bool QDeclarativeBluetoothService::isRegistered() const
{
    return d->m_service->isRegistered();
}

void QDeclarativeBluetoothService::setRegistered(bool registered)
{
    if (!d->m_componentComplete) {
        return;
    }

    delete d->m_server;
    d->m_server = 0;

    if (!registered) {
        d->m_service->unregisterService();
        emit registeredChanged();
        return;
    }

    if (d->m_protocol == UnknownProtocol) {
        qCWarning(QT_BT_QML) << "Unknown protocol, can't make service" << d->m_protocol;
        return;
    }

    QBluetoothServer *server
            = new QBluetoothServer(static_cast<QBluetoothServiceInfo::Protocol>(d->m_protocol));
    server->setMaxPendingConnections(1);
    if (!server->listen()) {
        qCWarning(QT_BT_QML) << "Could not start server. Error:" << server->error();
        return;
    }

    d->m_server = server;
    connect(d->m_server, SIGNAL(newConnection()), this, SLOT(new_connection()));

    d->m_service->setAttribute(QBluetoothServiceInfo::ServiceRecordHandle, (uint)0x00010010);

    QBluetoothServiceInfo::Sequence classId;
    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
    d->m_service->setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

    //qDebug() << "name/uuid" << d->m_name << d->m_uuid << d->m_port;

    d->m_service->setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                               QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;

    if (d->m_protocol == L2CapProtocol) {
        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap))
                 << QVariant::fromValue(quint16(d->m_server->serverPort()));
    } else if (d->m_protocol == RfcommProtocol) {
        //rfcomm implies l2cp protocol
        {
            QBluetoothServiceInfo::Sequence l2cpProtocol;
            l2cpProtocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
            protocolDescriptorList.append(QVariant::fromValue(l2cpProtocol));
        }
        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                 << QVariant::fromValue(quint8(d->m_server->serverPort()));
    }
    protocolDescriptorList.append(QVariant::fromValue(protocol));

    d->m_service->setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                               protocolDescriptorList);

    if (d->m_service->registerService())
        emit registeredChanged();
    else
        qCWarning(QT_BT_QML) << "Register service failed"; // TODO: propagate this error to the user
}

QBluetoothServiceInfo *QDeclarativeBluetoothService::serviceInfo() const
{
    return d->m_service;
}

void QDeclarativeBluetoothService::new_connection()
{
    emit newClient();
}

QDeclarativeBluetoothSocket *QDeclarativeBluetoothService::nextClient()
{
    QBluetoothServer *server = qobject_cast<QBluetoothServer *>(d->m_server);
    if (server) {
        if (server->hasPendingConnections()) {
            QBluetoothSocket *socket = server->nextPendingConnection();
            return new QDeclarativeBluetoothSocket(socket, this, 0);
        }
        else {
            qCWarning(QT_BT_QML) << "Socket has no pending connection, failing";
            return 0;
        }
    }
    return 0;
}

void QDeclarativeBluetoothService::assignNextClient(QDeclarativeBluetoothSocket *dbs)
{
    QBluetoothServer *server = qobject_cast<QBluetoothServer *>(d->m_server);
    if (server) {
        if (server->hasPendingConnections()) {
            QBluetoothSocket *socket = server->nextPendingConnection();
            dbs->newSocket(socket, this);
            return;
        }
        else {
            qCWarning(QT_BT_QML) << "Socket has no pending connection, failing";
            return;
        }
    }
    return;
}

