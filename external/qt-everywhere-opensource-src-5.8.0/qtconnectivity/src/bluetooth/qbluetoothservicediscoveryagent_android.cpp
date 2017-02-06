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

#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtBluetooth/QBluetoothHostInfo>
#include <QtBluetooth/QBluetoothLocalDevice>
#include <QtBluetooth/QBluetoothServiceDiscoveryAgent>

#include "qbluetoothservicediscoveryagent_p.h"
#include "android/servicediscoverybroadcastreceiver_p.h"
#include "android/localdevicebroadcastreceiver_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

QBluetoothServiceDiscoveryAgentPrivate::QBluetoothServiceDiscoveryAgentPrivate(
        QBluetoothServiceDiscoveryAgent *qp, const QBluetoothAddress &/*deviceAdapter*/)
    : error(QBluetoothServiceDiscoveryAgent::NoError),
      state(Inactive), deviceDiscoveryAgent(0),
      mode(QBluetoothServiceDiscoveryAgent::MinimalDiscovery),
      singleDevice(false), receiver(0), localDeviceReceiver(0),
      q_ptr(qp)

{
    QList<QBluetoothHostInfo> devices = QBluetoothLocalDevice::allDevices();
    Q_ASSERT(devices.count() <= 1); //Android only supports one device at the moment

    if (devices.isEmpty()) {
        error = QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Invalid Bluetooth adapter address");
        return;
    }

    if (QtAndroidPrivate::androidSdkVersion() < 15)
        qCWarning(QT_BT_ANDROID)
            << "SDP not supported by Android API below version 15. Detected version: "
            << QtAndroidPrivate::androidSdkVersion()
            << "Service discovery will return empty list.";


    /*
      We assume that the current local adapter has been passed.
      The logic below must change once there is more than one adapter.
    */

    btAdapter = QAndroidJniObject::callStaticObjectMethod("android/bluetooth/BluetoothAdapter",
                                                           "getDefaultAdapter",
                                                           "()Landroid/bluetooth/BluetoothAdapter;");
    if (!btAdapter.isValid())
        qCWarning(QT_BT_ANDROID) << "Platform does not support Bluetooth";

    qRegisterMetaType<QList<QBluetoothUuid> >();
}

QBluetoothServiceDiscoveryAgentPrivate::~QBluetoothServiceDiscoveryAgentPrivate()
{
    if (receiver) {
        receiver->unregisterReceiver();
        delete receiver;
    }
    if (localDeviceReceiver) {
        localDeviceReceiver->unregisterReceiver();
        delete localDeviceReceiver;
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::start(const QBluetoothAddress &address)
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (!btAdapter.isValid()) {
        error = QBluetoothServiceDiscoveryAgent::UnknownError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Platform does not support Bluetooth");

        //abort any outstanding discoveries
        discoveredDevices.clear();
        emit q->error(error);
        _q_serviceDiscoveryFinished();

        return;
    }

    /* SDP discovery was officially added by Android API v15
     * BluetoothDevice.getUuids() existed in earlier APIs already and in the future we may use
     * reflection to support earlier Android versions than 15. Unfortunately
     * BluetoothDevice.fetchUuidsWithSdp() and related APIs had some structure changes
     * over time. Therefore we won't attempt this with reflection.
     *
     * TODO: Use reflection to support getUuuids() where possible.
     * */
    if (QtAndroidPrivate::androidSdkVersion() < 15) {
        qCWarning(QT_BT_ANDROID) << "Aborting SDP enquiry due to too low Android API version (requires v15+)";

        error = QBluetoothServiceDiscoveryAgent::UnknownError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Android API below v15 does not support SDP discovery");

        //abort any outstanding discoveries
        sdpCache.clear();
        discoveredDevices.clear();
        emit q->error(error);
        _q_serviceDiscoveryFinished();

        return;
    }

    QAndroidJniObject inputString = QAndroidJniObject::fromString(address.toString());
    QAndroidJniObject remoteDevice =
            btAdapter.callObjectMethod("getRemoteDevice",
                                               "(Ljava/lang/String;)Landroid/bluetooth/BluetoothDevice;",
                                               inputString.object<jstring>());
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->ExceptionDescribe();

        //if it was only device then its error -> otherwise go to next device
        if (singleDevice) {
            error = QBluetoothServiceDiscoveryAgent::InputOutputError;
            errorString = QBluetoothServiceDiscoveryAgent::tr("Cannot create Android BluetoothDevice");

            qCWarning(QT_BT_ANDROID) << "Cannot start SDP for" << discoveredDevices.at(0).name()
                                     << "(" << address.toString() << ")";
            emit q->error(error);
        }
        _q_serviceDiscoveryFinished();
        return;
    }


    if (mode == QBluetoothServiceDiscoveryAgent::MinimalDiscovery) {
        qCDebug(QT_BT_ANDROID) << "Minimal discovery on (" << discoveredDevices.at(0).name()
                               << ")" << address.toString() ;

        //Minimal discovery uses BluetoothDevice.getUuids()
        QAndroidJniObject parcelUuidArray = remoteDevice.callObjectMethod(
                    "getUuids", "()[Landroid/os/ParcelUuid;");

        if (!parcelUuidArray.isValid()) {
            if (singleDevice) {
                error = QBluetoothServiceDiscoveryAgent::InputOutputError;
                errorString = QBluetoothServiceDiscoveryAgent::tr("Cannot obtain service uuids");
                emit q->error(error);
            }
            qCWarning(QT_BT_ANDROID) << "Cannot retrieve SDP UUIDs for" << discoveredDevices.at(0).name()
                                     << "(" << address.toString() << ")";
            _q_serviceDiscoveryFinished();
            return;
        }

        const QList<QBluetoothUuid> results = ServiceDiscoveryBroadcastReceiver::convertParcelableArray(parcelUuidArray);
        populateDiscoveredServices(discoveredDevices.at(0), results);

        _q_serviceDiscoveryFinished();
    } else {
        qCDebug(QT_BT_ANDROID) << "Full discovery on (" << discoveredDevices.at(0).name()
                               << ")" << address.toString();

        //Full discovery uses BluetoothDevice.fetchUuidsWithSdp()
        if (!receiver) {
            receiver = new ServiceDiscoveryBroadcastReceiver();
            QObject::connect(receiver, SIGNAL(uuidFetchFinished(QBluetoothAddress,QList<QBluetoothUuid>)),
                    q, SLOT(_q_processFetchedUuids(QBluetoothAddress,QList<QBluetoothUuid>)));
        }

        if (!localDeviceReceiver) {
            localDeviceReceiver = new LocalDeviceBroadcastReceiver();
            QObject::connect(localDeviceReceiver, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)),
                             q, SLOT(_q_hostModeStateChanged(QBluetoothLocalDevice::HostMode)));
        }

        jboolean result = remoteDevice.callMethod<jboolean>("fetchUuidsWithSdp");
        if (!result) {
            //kill receiver to limit load of signals
            receiver->unregisterReceiver();
            receiver->deleteLater();
            receiver = 0;
            qCWarning(QT_BT_ANDROID) << "Cannot start dynamic fetch.";
            _q_serviceDiscoveryFinished();
        }
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::stop()
{
    sdpCache.clear();
    discoveredDevices.clear();

    //kill receiver to limit load of signals
    receiver->unregisterReceiver();
    receiver->deleteLater();
    receiver = 0;

    Q_Q(QBluetoothServiceDiscoveryAgent);
    emit q->canceled();

}

void QBluetoothServiceDiscoveryAgentPrivate::_q_processFetchedUuids(
    const QBluetoothAddress &address, const QList<QBluetoothUuid> &uuids)
{
    //don't leave more data through if we are not interested anymore
    if (discoveredDevices.count() == 0)
        return;

    //could not find any service for the current address/device -> go to next one
    if (address.isNull() || uuids.isEmpty()) {
        _q_serviceDiscoveryFinished();
        return;
    }

    if (QT_BT_ANDROID().isDebugEnabled()) {
        qCDebug(QT_BT_ANDROID) << "Found UUID for" << address.toString()
                               << "\ncount: " << uuids.count();

        QString result;
        for (int i = 0; i<uuids.count(); i++)
            result += uuids.at(i).toString() + QStringLiteral("**");
        qCDebug(QT_BT_ANDROID) << result;
    }

    /* In general there are two uuid events per device.
     * We'll wait for the second event to arrive before we process the UUIDs.
     * We utilize a timeout to catch cases when the second
     * event doesn't arrive at all.
     * Generally we assume that the second uuid event carries the most up-to-date
     * set of uuids and discard the first events results.
    */

    if (sdpCache.contains(address)) {
        //second event
        QPair<QBluetoothDeviceInfo,QList<QBluetoothUuid> > pair = sdpCache.take(address);

        //prefer second uuid set over first
        populateDiscoveredServices(pair.first, uuids);

        if (discoveredDevices.count() == 1 && sdpCache.isEmpty()) {
            //last regular uuid data set from OS -> we finish here
            _q_serviceDiscoveryFinished();
        }
    } else {
        //first event
        QPair<QBluetoothDeviceInfo,QList<QBluetoothUuid> > pair;
        pair.first = discoveredDevices.at(0);
        pair.second = uuids;

        if (pair.first.address() != address)
            return;

        sdpCache.insert(address, pair);

        //the discovery on the last device cannot immediately finish
        //we have to grant the 2 seconds timeout delay
        if (discoveredDevices.count() == 1) {
            Q_Q(QBluetoothServiceDiscoveryAgent);
            QTimer::singleShot(4000, q, SLOT(_q_fetchUuidsTimeout()));
            return;
        }

        _q_serviceDiscoveryFinished();
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::populateDiscoveredServices(const QBluetoothDeviceInfo &remoteDevice, const QList<QBluetoothUuid> &uuids)
{
    /* Android doesn't provide decent SDP data. A list of uuids is close to meaning-less
     *
     * The following approach is chosen:
     * - If we see an SPP service class and we see
     * one or more custom uuids we match them up. Such services will always be SPP services.
     * - If we see a custom uuid but no SPP uuid then we return
     * BluetoothServiceInfo instance with just a servuceUuid (no service class set)
     * - Any other service uuid will stand on its own.
     * */

    Q_Q(QBluetoothServiceDiscoveryAgent);

    //find SPP and custom uuid
    QBluetoothUuid uuid;
    int sppIndex = -1;
    QVector<int> customUuids;

    for (int i = 0; i < uuids.count(); i++) {
        uuid = uuids.at(i);

        if (uuid.isNull())
            continue;

        //check for SPP protocol
        bool ok = false;
        quint16 uuid16 = uuid.toUInt16(&ok);
        if (ok && uuid16 == QBluetoothUuid::SerialPort)
            sppIndex = i;

        //check for custom uuid
        if (uuid.minimumSize() == 16)
            customUuids.append(i);
    }

    for (int i = 0; i < uuids.count(); i++) {
        if (i == sppIndex && !customUuids.isEmpty())
            continue;

        QBluetoothServiceInfo serviceInfo;
        serviceInfo.setDevice(remoteDevice);

        QBluetoothServiceInfo::Sequence protocolDescriptorList;
        {
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
            protocolDescriptorList.append(QVariant::fromValue(protocol));
        }

        if (customUuids.contains(i) && sppIndex > -1) {
            //we have a custom uuid of service class type SPP

            //set rfcomm protocol
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                     << QVariant::fromValue(0);
            protocolDescriptorList.append(QVariant::fromValue(protocol));

            //set SPP service class uuid
            QBluetoothServiceInfo::Sequence classId;
            classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
            serviceInfo.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList,
                                     classId);
            classId.prepend(QVariant::fromValue(uuids.at(i)));
            serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

            serviceInfo.setServiceName(QBluetoothServiceDiscoveryAgent::tr("Serial Port Profile"));
            serviceInfo.setServiceUuid(uuids.at(i));
        } else if (sppIndex == i && customUuids.isEmpty()) {
            //set rfcomm protocol
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                     << QVariant::fromValue(0);
            protocolDescriptorList.append(QVariant::fromValue(protocol));

            QBluetoothServiceInfo::Sequence classId;
            classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
            serviceInfo.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList,
                                     classId);

            //also we need to set the custom uuid to the SPP uuid
            //otherwise QBluetoothSocket::connectToService() would fail due to a missing service uuid
            serviceInfo.setServiceUuid(uuids.at(i));
        } else if (customUuids.contains(i)) {
            //custom uuid but no serial port
            serviceInfo.setServiceUuid(uuids.at(i));
        }

        //Check if the UUID is in the uuidFilter
        if (!uuidFilter.isEmpty() && !uuidFilter.contains(serviceInfo.serviceUuid()))
            continue;

        serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);
        serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                                 QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

        if (!customUuids.contains(i)) {
            //if we don't have custom uuid use it as class id as well
            QBluetoothServiceInfo::Sequence classId;
            classId << QVariant::fromValue(uuids.at(i));
            serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
            QBluetoothUuid::ServiceClassUuid clsId
                = static_cast<QBluetoothUuid::ServiceClassUuid>(uuids.at(i).toUInt16());
            serviceInfo.setServiceName(QBluetoothUuid::serviceClassToString(clsId));
        }

        //don't include the service if we already discovered it before
        if (!isDuplicatedService(serviceInfo)) {
            discoveredServices << serviceInfo;
            //qCDebug(QT_BT_ANDROID) << serviceInfo;
            emit q->serviceDiscovered(serviceInfo);
        }
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_fetchUuidsTimeout()
{
    if (sdpCache.isEmpty())
        return;

    QPair<QBluetoothDeviceInfo,QList<QBluetoothUuid> > pair;
    const QList<QBluetoothAddress> keys = sdpCache.keys();
    foreach (const QBluetoothAddress &key, keys) {
        pair = sdpCache.take(key);
        populateDiscoveredServices(pair.first, pair.second);
    }

    Q_ASSERT(sdpCache.isEmpty());

    //kill receiver to limit load of signals
    receiver->unregisterReceiver();
    receiver->deleteLater();
    receiver = 0;
    _q_serviceDiscoveryFinished();
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_hostModeStateChanged(QBluetoothLocalDevice::HostMode state)
{
    if (discoveryState() == QBluetoothServiceDiscoveryAgentPrivate::ServiceDiscovery &&
            state == QBluetoothLocalDevice::HostPoweredOff ) {

        discoveredDevices.clear();
        sdpCache.clear();
        error = QBluetoothServiceDiscoveryAgent::PoweredOffError;
        errorString = QBluetoothServiceDiscoveryAgent::tr("Device is powered off");

        //kill receiver to limit load of signals
        receiver->unregisterReceiver();
        receiver->deleteLater();
        receiver = 0;

        Q_Q(QBluetoothServiceDiscoveryAgent);
        emit q->error(error);
        _q_serviceDiscoveryFinished();
    }
}

QT_END_NAMESPACE
