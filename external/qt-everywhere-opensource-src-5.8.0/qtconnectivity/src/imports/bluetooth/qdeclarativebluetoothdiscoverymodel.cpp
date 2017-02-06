/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
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
        m_componentCompleted(false),
        m_currentState(QDeclarativeBluetoothDiscoveryModel::IdleAction),
        m_nextState(QDeclarativeBluetoothDiscoveryModel::IdleAction),
        m_wasDirectDeviceAgentCancel(false)
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

    QDeclarativeBluetoothDiscoveryModel::Action m_currentState;
    QDeclarativeBluetoothDiscoveryModel::Action m_nextState;
    bool m_wasDirectDeviceAgentCancel;
};

QDeclarativeBluetoothDiscoveryModel::QDeclarativeBluetoothDiscoveryModel(QObject *parent) :
    QAbstractListModel(parent),
    d(new QDeclarativeBluetoothDiscoveryModelPrivate)
{
    d->m_deviceAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(d->m_deviceAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));
    connect(d->m_deviceAgent, SIGNAL(finished()), this, SLOT(finishedDiscovery()));
    connect(d->m_deviceAgent, SIGNAL(canceled()), this, SLOT(finishedDiscovery()));
    connect(d->m_deviceAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
            this, SLOT(errorDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::Error)));
    d->m_deviceAgent->setObjectName("DeviceDiscoveryAgent");

    d->m_serviceAgent = new QBluetoothServiceDiscoveryAgent(this);
    connect(d->m_serviceAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
    connect(d->m_serviceAgent, SIGNAL(finished()), this, SLOT(finishedDiscovery()));
    connect(d->m_serviceAgent, SIGNAL(canceled()), this, SLOT(finishedDiscovery()));
    connect(d->m_serviceAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)),
            this, SLOT(errorDiscovery(QBluetoothServiceDiscoveryAgent::Error)));
    d->m_serviceAgent->setObjectName("ServiceDiscoveryAgent");

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
    //This resets the models running flag.
    finishedDiscovery();
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
  the Bluetooth address of the discovered device.

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
    QDeclarativeBluetoothDiscoveryModel::Action previous = d->m_currentState;
    d->m_currentState = IdleAction;

    switch (previous) {
    case IdleAction:
        // last transition didn't even start
        // can happen when start() or stop() immediately returned
        // usually this happens within a current transitionToNextAction call
        break;
    case StopAction:
        qCDebug(QT_BT_QML) << "Agent cancel detected";
        transitionToNextAction();
        break;
    default: // all other
        qCDebug(QT_BT_QML) << "Discovery finished" << sender()->objectName();

        //TODO Qt6 This hack below is once again due to the pendingCancel logic
        //         because QBluetoothDeviceDiscoveryAgent::isActive() is not reliable.
        //         In toggleStartStop() we need to know whether the stop() is delayed or immediate.
        //         isActive() cannot be used. Hence we have to wait for the canceled() signal.
        //         Android, WinRT and Bluez5 are immediate, Bluez4 is always delayed.
        //         The immediate case is what we catch here.
        if (sender() == d->m_deviceAgent && d->m_nextState == StopAction) {
            d->m_wasDirectDeviceAgentCancel = true;
            return;
        }
        setRunning(false);
        break;
    }
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


void QDeclarativeBluetoothDiscoveryModel::updateNextAction(Action action)
{
    qCDebug(QT_BT_QML) << "New action queue:"
                       << d->m_currentState << d->m_nextState << action;

    if (action == IdleAction)
        return;

    switch (d->m_nextState) {
    case IdleAction:
        d->m_nextState = action;
        return;
    case StopAction:
        qWarning() << "Invalid Stop state when processing new action" << action;
        return;
    case DeviceDiscoveryAction:
    case MinimalServiceDiscoveryAction:
    case FullServiceDiscoveryAction:
        if (action == StopAction) // cancel out previous start call
            d->m_nextState = IdleAction;
        else
            qWarning() << "Ignoring new DMF state while another DMF state is scheduled.";
        return;
    }
}

void QDeclarativeBluetoothDiscoveryModel::transitionToNextAction()
{
    qCDebug(QT_BT_QML) << "Before transition change:" << d->m_currentState << d->m_nextState;
    bool isRunning;
    switch (d->m_currentState) {
    case IdleAction:
        switch (d->m_nextState) {
        case IdleAction: break; // nothing to do
        case StopAction: d->m_nextState = IdleAction; break; // clear, nothing to do
        case DeviceDiscoveryAction:
        case MinimalServiceDiscoveryAction:
        case FullServiceDiscoveryAction:
            Action temp = d->m_nextState;
            clearModel();
            isRunning = toggleStartStop(d->m_nextState);
            d->m_nextState = IdleAction;
            if (isRunning) {
                d->m_currentState = temp;
            } else {
                if (temp != DeviceDiscoveryAction )
                    errorDiscovery(d->m_serviceAgent->error());
                d->m_running = false;
            }
        }
        break;
    case StopAction:
        break; // do nothing, StopAction cleared by finished()/cancelled()/error() handlers
    case DeviceDiscoveryAction:
    case MinimalServiceDiscoveryAction:
    case FullServiceDiscoveryAction:
        switch (d->m_nextState) {
        case IdleAction: break;
        case StopAction:
            isRunning = toggleStartStop(StopAction);
            (isRunning) ? d->m_currentState = StopAction : d->m_currentState = IdleAction;
            d->m_nextState = IdleAction;
            break;
         default:
            Q_ASSERT(false); // should never happen
            break;
        }

        break;
    }

    qCDebug(QT_BT_QML) << "After transition change:" << d->m_currentState << d->m_nextState;
}

// Returns true if the agent is active
// this can be used to detect whether the agent still needs time to
// perform the requested action.
bool QDeclarativeBluetoothDiscoveryModel::toggleStartStop(Action action)
{
    Q_ASSERT(action != IdleAction);
    switch (action) {
    case DeviceDiscoveryAction:
        Q_ASSERT(!d->m_deviceAgent->isActive() && !d->m_serviceAgent->isActive());
        d->m_deviceAgent->start();
        return d->m_deviceAgent->isActive();
    case MinimalServiceDiscoveryAction:
    case FullServiceDiscoveryAction:
        Q_ASSERT(!d->m_deviceAgent->isActive() && !d->m_serviceAgent->isActive());
        d->m_serviceAgent->setRemoteAddress(QBluetoothAddress(d->m_remoteAddress));
        d->m_serviceAgent->clear();

        if (!d->m_uuid.isEmpty())
            d->m_serviceAgent->setUuidFilter(QBluetoothUuid(d->m_uuid));

        if (action == FullServiceDiscoveryAction)  {
            qCDebug(QT_BT_QML) << "Full Discovery";
            d->m_serviceAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
        } else {
            qCDebug(QT_BT_QML) << "Minimal Discovery";
            d->m_serviceAgent->start(QBluetoothServiceDiscoveryAgent::MinimalDiscovery);
        }
        return d->m_serviceAgent->isActive();
    case StopAction:
        Q_ASSERT(d->m_currentState != StopAction && d->m_currentState != IdleAction);
        if (d->m_currentState == DeviceDiscoveryAction) {
            d->m_deviceAgent->stop();

            // TODO Qt6 Crude hack below
            // cannot use isActive() below due to pendingCancel logic
            // we always wait for canceled() signal coming through or check
            // for directly invoked cancel() response caused by stop() above
            bool stillActive = !d->m_wasDirectDeviceAgentCancel;
            d->m_wasDirectDeviceAgentCancel = false;
            return stillActive;
        } else {
            d->m_serviceAgent->stop();
            return d->m_serviceAgent->isActive();
        }
    default:
        return true;
    }
}


/*!
  \qmlproperty bool BluetoothDiscoveryModel::running

  This property starts or stops discovery. A restart of the discovery process
  requires setting this property to \c false and subsequently to \c true again.

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

    Action nextAction = IdleAction;
    if (running) {
        if (discoveryMode() == MinimalServiceDiscovery)
            nextAction = MinimalServiceDiscoveryAction;
        else if (discoveryMode() == FullServiceDiscovery)
            nextAction = FullServiceDiscoveryAction;
        else
            nextAction = DeviceDiscoveryAction;
    } else {
        nextAction = StopAction;
    }

    Q_ASSERT(nextAction != IdleAction);
    updateNextAction(nextAction);
    transitionToNextAction();

    qCDebug(QT_BT_QML) << "Running state:" << d->m_running;
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
