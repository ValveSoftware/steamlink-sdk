/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qdeclarativebluetoothdiscoverymodel_p.h"

#include <QPixmap>

#include <QtCore/QLoggingCategory>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothAddress>

#include "qdeclarativebluetoothservice_p.h"

/*!
    \qmltype BluetoothDiscoveryModel
    \instantiates QDeclarativeBluetoothDiscoveryModel
    \inqmlmodule QtBluetooth
    \since 5.2
    \brief Enables searching for the Bluetooth devices and services in
    range.

    BluetoothDiscoveryModel provides a model of connectable services. The
    contents of the model can be filtered by UUID allowing discovery to be
    limited to a single service such as a game.

    The model roles provided by BluetoothDiscoveryModel are
    \c service, \c name, \c remoteAddress and \c deviceName. The meaning of the roles
    changes based on the current \l discoveryMode.

    \table
        \header
            \li Model role
            \li Device Discovery
            \li Service Discovery
         \row
            \li \c name
            \li The device's name and address.
            \li The service name and the name of the device offering the service. If the device name is empty the devices address will be used.
         \row
            \li \c deviceName
            \li The name of the device.
            \li The name of the device offering the service.
         \row
            \li \c service
            \li The role is undefined in this mode.
            \li The \l BluetoothService object describing the discovered service.
         \row
            \li \c remoteAddress
            \li The address of the found device.
            \li The address of the device offering the service.
    \endtable

    \sa QBluetoothServiceDiscoveryAgent
*/

Q_DECLARE_LOGGING_CATEGORY(QT_BT_QML)

class QDeclarativeBluetoothDiscoveryModelPrivate
{
public:
    QDeclarativeBluetoothDiscoveryModelPrivate()
        : m_serviceAgent(0),
        m_deviceAgent(0),
        m_error(QDeclarativeBluetoothDiscoveryModel::NoError),
        m_discoveryMode(QDeclarativeBluetoothDiscoveryModel::MinimalServiceDiscovery),
        m_running(false),
        m_runningRequested(true),
        m_componentCompleted(false)
    {
    }
    ~QDeclarativeBluetoothDiscoveryModelPrivate()
    {
        if (m_deviceAgent)
            delete m_deviceAgent;

        if (m_serviceAgent)
            delete m_serviceAgent;

        qDeleteAll(m_services);
    }

    QBluetoothServiceDiscoveryAgent *m_serviceAgent;
    QBluetoothDeviceDiscoveryAgent *m_deviceAgent;

    QDeclarativeBluetoothDiscoveryModel::Error m_error;
    QList<QDeclarativeBluetoothService *> m_services;
    QList<QBluetoothDeviceInfo> m_devices;
    QDeclarativeBluetoothDiscoveryModel::DiscoveryMode m_discoveryMode;
    QString m_uuid;
    bool m_running;
    bool m_runningRequested;
    bool m_componentCompleted;
    QString m_remoteAddress;
};

QDeclarativeBluetoothDiscoveryModel::QDeclarativeBluetoothDiscoveryModel(QObject *parent) :
    QAbstractListModel(parent),
    d(new QDeclarativeBluetoothDiscoveryModelPrivate)
{

    QHash<int, QByteArray> roleNames;
    roleNames = QAbstractItemModel::roleNames();
    roleNames.insert(Name, "name");
    roleNames.insert(ServiceRole, "service");
    roleNames.insert(RemoteAddress, "remoteAddress");
    roleNames.insert(DeviceName, "deviceName");
    setRoleNames(roleNames);
}

QDeclarativeBluetoothDiscoveryModel::~QDeclarativeBluetoothDiscoveryModel()
{
    delete d;
}
void QDeclarativeBluetoothDiscoveryModel::componentComplete()
{
    d->m_componentCompleted = true;
    if (d->m_runningRequested)
        setRunning(true);
}

void QDeclarativeBluetoothDiscoveryModel::errorDiscovery(QBluetoothServiceDiscoveryAgent::Error error)
{
    switch (error) {
    case QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError:
        d->m_error = QDeclarativeBluetoothDiscoveryModel::InvalidBluetoothAdapterError; break;
    case QBluetoothServiceDiscoveryAgent::NoError:
        d->m_error = QDeclarativeBluetoothDiscoveryModel::NoError; break;
    case QBluetoothServiceDiscoveryAgent::InputOutputError:
        d->m_error = QDeclarativeBluetoothDiscoveryModel::InputOutputError; break;
    case QBluetoothServiceDiscoveryAgent::PoweredOffError:
        d->m_error = QDeclarativeBluetoothDiscoveryModel::PoweredOffError; break;
    case QBluetoothServiceDiscoveryAgent::UnknownError:
        d->m_error = QDeclarativeBluetoothDiscoveryModel::UnknownError; break;
    }

    emit errorChanged();
}

void QDeclarativeBluetoothDiscoveryModel::errorDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::Error error)
{
    d->m_error = static_cast<QDeclarativeBluetoothDiscoveryModel::Error>(error);
    emit errorChanged();

    //QBluetoothDeviceDiscoveryAgent::finished() signal is not emitted in case of an error
    //Note that this behavior is different from QBluetoothServiceDiscoveryAgent.
    //This reset the models running flag.
    setRunning(false);
}

void QDeclarativeBluetoothDiscoveryModel::clearModel()
{
    beginResetModel();
    qDeleteAll(d->m_services);
    d->m_services.clear();
    d->m_devices.clear();
    endResetModel();
}

/*!
    \qmlproperty enumeration BluetoothDiscoveryModel::error

    This property holds the last error reported during discovery.
    \table
    \header \li Property \li Description
    \row \li \c BluetoothDiscoveryModel.NoError
         \li No error occurred.
    \row \li \c BluetoothDiscoveryModel.InputOutputError
         \li An IO failure occurred during device discovery
    \row \li \c BluetoothDiscoveryModel.PoweredOffError
         \li The bluetooth device is not powered on.
    \row \li \c BluetoothDiscoveryModel.InvalidBluetoothAdapterError
         \li There is no default Bluetooth device to perform the
             service discovery. The model always uses the local default adapter.
             Specifying a default adapter is not possible. If that's required,
             \l QBluetoothServiceDiscoveryAgent should be directly used. This
             value was introduced by Qt 5.4.
    \row \li \c BluetoothDiscoveryModel.UnknownError
         \li An unknown error occurred.
    \endtable

  This property is read-only.
  */
QDeclarativeBluetoothDiscoveryModel::Error QDeclarativeBluetoothDiscoveryModel::error() const
{
    return d->m_error;
}

int QDeclarativeBluetoothDiscoveryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (discoveryMode() == DeviceDiscovery)
        return d->m_devices.count();
    else
        return d->m_services.count();
}

QVariant QDeclarativeBluetoothDiscoveryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0)
        return QVariant();

    if (discoveryMode() != DeviceDiscovery) {
        if (index.row() >= d->m_services.count()){
            qCWarning(QT_BT_QML) << "index out of bounds";
            return QVariant();
        }

        QDeclarativeBluetoothService *service = d->m_services.value(index.row());

        switch (role) {
            case Name: {
                QString label = service->deviceName();
                if (label.isEmpty())
                    label += service->deviceAddress();
                else
                    label+= QStringLiteral(":");
                label += QStringLiteral(" ") + service->serviceName();
                return label;
            }
            case ServiceRole:
                return QVariant::fromValue(service);
            case DeviceName:
                return service->deviceName();
            case RemoteAddress:
                return service->deviceAddress();
        }
    } else {
        if (index.row() >= d->m_devices.count()) {
            qCWarning(QT_BT_QML) << "index out of bounds";
            return QVariant();
        }

        QBluetoothDeviceInfo device = d->m_devices.value(index.row());
        switch (role) {
            case Name:
            return (device.name() + QStringLiteral(" (") + device.address().toString() + QStringLiteral(")"));
            case ServiceRole:
                break;
            case DeviceName:
                return device.name();
            case RemoteAddress:
                return device.address().toString();
        }
    }

    return QVariant();
}

/*!
  \qmlsignal BluetoothDiscoveryModel::serviceDiscovered(BluetoothService service)

  This signal is emitted when a new service is discovered. The \a service
  parameter contains the service details.

  The corresponding handler is \c onServiceDiscovered.

  \sa BluetoothService
  */

void QDeclarativeBluetoothDiscoveryModel::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    //qDebug() << "service discovered";

    QDeclarativeBluetoothService *bs = new QDeclarativeBluetoothService(service, this);
    QDeclarativeBluetoothService *current = 0;
    for (int i = 0; i < d->m_services.count(); i++) {
        current = d->m_services.at(i);
        if (bs->deviceAddress() == current->deviceAddress()
                && bs->serviceName() == current->serviceName()
                && bs->serviceUuid() == current->serviceUuid()) {
            delete bs;
            return;
        }
    }

    beginInsertRows(QModelIndex(),d->m_services.count(), d->m_services.count());
    d->m_services.append(bs);
    endInsertRows();
    emit serviceDiscovered(bs);
}

/*!
  \qmlsignal BluetoothDiscoveryModel::deviceDiscovered(string device)

  This signal is emitted when a new device is discovered. \a device contains
  the Bluetooth address of the discovred device.

  The corresponding handler is \c onDeviceDiscovered.
  */

void QDeclarativeBluetoothDiscoveryModel::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    //qDebug() << "Device discovered" << device.address().toString() << device.name();

    beginInsertRows(QModelIndex(),d->m_devices.count(), d->m_devices.count());
    d->m_devices.append(device);
    endInsertRows();
    emit deviceDiscovered(device.address().toString());
}

void QDeclarativeBluetoothDiscoveryModel::finishedDiscovery()
{
    setRunning(false);
}

/*!
    \qmlproperty enumeration BluetoothDiscoveryModel::discoveryMode

    Sets the discovery mode. The discovery mode has to be set before the discovery is started
    \table
    \header \li Property \li Description
    \row \li \c BluetoothDiscoveryModel.FullServiceDiscovery
         \li Starts a full discovery of all services of all devices in range.
    \row \li \c BluetoothDiscoveryModel.MinimalServiceDiscovery
         \li (Default) Starts a minimal discovery of all services of all devices in range. A minimal discovery is
         faster but only guarantees the device and UUID information to be correct.
    \row \li \c BluetoothDiscoveryModel.DeviceDiscovery
         \li Discovers only devices in range. The service role will be 0 for any model item.
    \endtable
*/

QDeclarativeBluetoothDiscoveryModel::DiscoveryMode QDeclarativeBluetoothDiscoveryModel::discoveryMode() const
{
    return d->m_discoveryMode;
}

void QDeclarativeBluetoothDiscoveryModel::setDiscoveryMode(DiscoveryMode discovery)
{
    d->m_discoveryMode = discovery;
    emit discoveryModeChanged();
}

/*!
  \qmlproperty bool BluetoothDiscoveryModel::running

  This property starts or stops discovery. A restart of the discovery process
  requires setting this property to \c false and subsequemtly to \c true again.

*/

bool QDeclarativeBluetoothDiscoveryModel::running() const
{
    return d->m_running;
}

void QDeclarativeBluetoothDiscoveryModel::setRunning(bool running)
{
    if (!d->m_componentCompleted) {
        d->m_runningRequested = running;
        return;
    }

    if (d->m_running == running)
        return;

    d->m_running = running;

    if (!running) {
        if (d->m_deviceAgent)
            d->m_deviceAgent->stop();
        if (d->m_serviceAgent)
            d->m_serviceAgent->stop();
    } else {
        clearModel();
        d->m_error = NoError;
        if (d->m_discoveryMode == DeviceDiscovery) {
            if (!d->m_deviceAgent) {
                d->m_deviceAgent = new QBluetoothDeviceDiscoveryAgent(this);
                connect(d->m_deviceAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)), this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));
                connect(d->m_deviceAgent, SIGNAL(finished()), this, SLOT(finishedDiscovery()));
                connect(d->m_deviceAgent, SIGNAL(canceled()), this, SLOT(finishedDiscovery()));
                connect(d->m_deviceAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)), this, SLOT(errorDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::Error)));
            }
            d->m_deviceAgent->start();
        } else {
            if (!d->m_serviceAgent) {
                d->m_serviceAgent = new QBluetoothServiceDiscoveryAgent(this);
                connect(d->m_serviceAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)), this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
                connect(d->m_serviceAgent, SIGNAL(finished()), this, SLOT(finishedDiscovery()));
                connect(d->m_serviceAgent, SIGNAL(canceled()), this, SLOT(finishedDiscovery()));
                connect(d->m_serviceAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)), this, SLOT(errorDiscovery(QBluetoothServiceDiscoveryAgent::Error)));
            }

            d->m_serviceAgent->setRemoteAddress(QBluetoothAddress(d->m_remoteAddress));
            d->m_serviceAgent->clear();

            if (!d->m_uuid.isEmpty())
                d->m_serviceAgent->setUuidFilter(QBluetoothUuid(d->m_uuid));

            if (discoveryMode() == FullServiceDiscovery)  {
                //qDebug() << "Full Discovery";
                d->m_serviceAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
            } else {
                //qDebug() << "Minimal Discovery";
                d->m_serviceAgent->start(QBluetoothServiceDiscoveryAgent::MinimalDiscovery);
            }

            // we could not start service discovery
            if (!d->m_serviceAgent->isActive()) {
                d->m_running = false;
                errorDiscovery(d->m_serviceAgent->error());
                return;
            }
        }
    }

    emit runningChanged();
}

/*!
  \qmlproperty string BluetoothDiscoveryModel::uuidFilter

  This property holds an optional UUID filter.  A UUID can be used to return only
  matching services.  16 bit, 32 bit or 128 bit UUIDs can be used.  The string format
  is same as the format of QUuid.

  \sa QBluetoothUuid
  \sa QUuid
  */


QString QDeclarativeBluetoothDiscoveryModel::uuidFilter() const
{
    return d->m_uuid;
}

void QDeclarativeBluetoothDiscoveryModel::setUuidFilter(QString uuid)
{
    if (uuid == d->m_uuid)
        return;

    QBluetoothUuid qbuuid(uuid);
    if (qbuuid.isNull()) {
        qCWarning(QT_BT_QML) << "Invalid UUID providded " << uuid;
        return;
    }
    d->m_uuid = uuid;
    emit uuidFilterChanged();
}

/*!
    \qmlproperty string BluetoothDiscoveryModel::remoteAddress

    This property holds an optional bluetooth address for a remote bluetooth device.
    Only services on this remote device will be discovered. It has no effect if
    an invalid bluetooth address was set or if the property was set after the discovery
    was started.

    The property is ignored if device discovery is selected.

*/

QString QDeclarativeBluetoothDiscoveryModel::remoteAddress()
{
    return d->m_remoteAddress;
}

void QDeclarativeBluetoothDiscoveryModel::setRemoteAddress(QString address)
{
    d->m_remoteAddress = address;
    emit remoteAddressChanged();
}
