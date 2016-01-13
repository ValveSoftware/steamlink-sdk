/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include <QtCore/QLoggingCategory>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtBluetooth/QBluetoothLocalDevice>
#include <QtBluetooth/QBluetoothAddress>

#include "qbluetoothlocaldevice_p.h"
#include "android/localdevicebroadcastreceiver_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

QBluetoothLocalDevicePrivate::QBluetoothLocalDevicePrivate(
    QBluetoothLocalDevice *q, const QBluetoothAddress &address) :
    q_ptr(q),
    obj(0),
    pendingHostModeTransition(false)
{
    initialize(address);

    receiver = new LocalDeviceBroadcastReceiver(q_ptr);
    connect(receiver, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)),
            this, SLOT(processHostModeChange(QBluetoothLocalDevice::HostMode)));
    connect(receiver, SIGNAL(pairingStateChanged(QBluetoothAddress,
                                                 QBluetoothLocalDevice::Pairing)),
            this, SLOT(processPairingStateChanged(QBluetoothAddress,
                                                  QBluetoothLocalDevice::Pairing)));
    connect(receiver, SIGNAL(connectDeviceChanges(QBluetoothAddress, bool)),
            this, SLOT(processConnectDeviceChanges(QBluetoothAddress, bool)));
    connect(receiver, SIGNAL(pairingDisplayConfirmation(QBluetoothAddress, QString)),
            this, SLOT(processDisplayConfirmation(QBluetoothAddress, QString)));
}

QBluetoothLocalDevicePrivate::~QBluetoothLocalDevicePrivate()
{
    receiver->unregisterReceiver();
    delete receiver;
    delete obj;
}

QAndroidJniObject *QBluetoothLocalDevicePrivate::adapter()
{
    return obj;
}

void QBluetoothLocalDevicePrivate::initialize(const QBluetoothAddress &address)
{
    QAndroidJniEnvironment env;

    jclass btAdapterClass = env->FindClass("android/bluetooth/BluetoothAdapter");
    if (btAdapterClass == NULL) {
        qCWarning(QT_BT_ANDROID)
            << "Native registration unable to find class android/bluetooth/BluetoothAdapter";
        return;
    }

    jmethodID getDefaultAdapterID
        = env->GetStaticMethodID(btAdapterClass, "getDefaultAdapter",
                                 "()Landroid/bluetooth/BluetoothAdapter;");
    if (getDefaultAdapterID == NULL) {
        qCWarning(QT_BT_ANDROID)
            << "Native registration unable to get method ID: " \
               "getDefaultAdapter of android/bluetooth/BluetoothAdapter";
        return;
    }

    jobject btAdapterObject = env->CallStaticObjectMethod(btAdapterClass, getDefaultAdapterID);
    if (btAdapterObject == NULL) {
        qCWarning(QT_BT_ANDROID) <<  "Device does not support Bluetooth";
        env->DeleteLocalRef(btAdapterClass);
        return;
    }

    obj = new QAndroidJniObject(btAdapterObject);
    if (!obj->isValid()) {
        delete obj;
        obj = 0;
    } else if (!address.isNull()) {
        const QString localAddress
            = obj->callObjectMethod("getAddress", "()Ljava/lang/String;").toString();
        if (localAddress != address.toString()) {
            // passed address not local one -> invalid
            delete obj;
            obj = 0;
        }
    }

    env->DeleteLocalRef(btAdapterObject);
    env->DeleteLocalRef(btAdapterClass);
}

bool QBluetoothLocalDevicePrivate::isValid() const
{
    return obj ? true : false;
}

void QBluetoothLocalDevicePrivate::processHostModeChange(QBluetoothLocalDevice::HostMode newMode)
{
    if (!pendingHostModeTransition) {
        // if not in transition -> pass data on
        emit q_ptr->hostModeStateChanged(newMode);
        return;
    }

    if (isValid() && newMode == QBluetoothLocalDevice::HostPoweredOff) {
        bool success = (bool)obj->callMethod<jboolean>("enable", "()Z");
        if (!success)
            emit q_ptr->error(QBluetoothLocalDevice::UnknownError);
    }

    pendingHostModeTransition = false;
}

// Return -1 if address is not part of a pending pairing request
// Otherwise it returns the index of address in pendingPairings
int QBluetoothLocalDevicePrivate::pendingPairing(const QBluetoothAddress &address)
{
    for (int i = 0; i < pendingPairings.count(); i++) {
        if (pendingPairings.at(i).first == address)
            return i;
    }

    return -1;
}

void QBluetoothLocalDevicePrivate::processPairingStateChanged(
    const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
    int index = pendingPairing(address);

    if (index < 0)
        return; // ignore unrelated pairing signals

    QPair<QBluetoothAddress, bool> entry = pendingPairings.takeAt(index);
    if ((entry.second && pairing == QBluetoothLocalDevice::Paired)
        || (!entry.second && pairing == QBluetoothLocalDevice::Unpaired)) {
        emit q_ptr->pairingFinished(address, pairing);
    } else {
        emit q_ptr->error(QBluetoothLocalDevice::PairingError);
    }
}

void QBluetoothLocalDevicePrivate::processConnectDeviceChanges(const QBluetoothAddress &address,
                                                               bool isConnectEvent)
{
    int index = -1;
    for (int i = 0; i < connectedDevices.count(); i++) {
        if (connectedDevices.at(i) == address) {
            index = i;
            break;
        }
    }

    if (isConnectEvent) { // connect event
        if (index >= 0)
            return;
        connectedDevices.append(address);
        emit q_ptr->deviceConnected(address);
    } else { // disconnect event
        connectedDevices.removeAll(address);
        emit q_ptr->deviceDisconnected(address);
    }
}

void QBluetoothLocalDevicePrivate::processDisplayConfirmation(const QBluetoothAddress &address,
                                                              const QString &pin)
{
    // only send pairing notification for pairing requests issued by
    // this QBluetoothLocalDevice instance
    if (pendingPairing(address) == -1)
        return;

    emit q_ptr->pairingDisplayConfirmation(address, pin);
    emit q_ptr->pairingDisplayPinCode(address, pin);
}

QBluetoothLocalDevice::QBluetoothLocalDevice(QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothLocalDevicePrivate(this, QBluetoothAddress()))
{
}

QBluetoothLocalDevice::QBluetoothLocalDevice(const QBluetoothAddress &address, QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothLocalDevicePrivate(this, address))
{
}

QString QBluetoothLocalDevice::name() const
{
    if (d_ptr->adapter())
        return d_ptr->adapter()->callObjectMethod("getName", "()Ljava/lang/String;").toString();

    return QString();
}

QBluetoothAddress QBluetoothLocalDevice::address() const
{
    QString result;
    if (d_ptr->adapter()) {
        result
            = d_ptr->adapter()->callObjectMethod("getAddress", "()Ljava/lang/String;").toString();
    }

    QBluetoothAddress address(result);
    return address;
}

void QBluetoothLocalDevice::powerOn()
{
    if (hostMode() != HostPoweredOff)
        return;

    if (d_ptr->adapter()) {
        bool ret = (bool)d_ptr->adapter()->callMethod<jboolean>("enable", "()Z");
        if (!ret)
            emit error(QBluetoothLocalDevice::UnknownError);
    }
}

void QBluetoothLocalDevice::setHostMode(QBluetoothLocalDevice::HostMode requestedMode)
{
    QBluetoothLocalDevice::HostMode mode = requestedMode;
    if (requestedMode == HostDiscoverableLimitedInquiry)
        mode = HostDiscoverable;

    if (mode == hostMode())
        return;

    if (mode == QBluetoothLocalDevice::HostPoweredOff) {
        bool success = false;
        if (d_ptr->adapter())
            success = (bool)d_ptr->adapter()->callMethod<jboolean>("disable", "()Z");

        if (!success)
            emit error(QBluetoothLocalDevice::UnknownError);
    } else if (mode == QBluetoothLocalDevice::HostConnectable) {
        if (hostMode() == QBluetoothLocalDevice::HostDiscoverable) {
            // cannot directly go from Discoverable to Connectable
            // we need to go to disabled mode and enable once disabling came through

            setHostMode(QBluetoothLocalDevice::HostPoweredOff);
            d_ptr->pendingHostModeTransition = true;
        } else {
            QAndroidJniObject::callStaticMethod<void>(
                "org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver",
                "setConnectable");
        }
    } else if (mode == QBluetoothLocalDevice::HostDiscoverable
               || mode == QBluetoothLocalDevice::HostDiscoverableLimitedInquiry) {
        QAndroidJniObject::callStaticMethod<void>(
            "org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver", "setDiscoverable");
    }
}

QBluetoothLocalDevice::HostMode QBluetoothLocalDevice::hostMode() const
{
    if (d_ptr->adapter()) {
        jint scanMode = d_ptr->adapter()->callMethod<jint>("getScanMode");

        switch (scanMode) {
        case 20:     // BluetoothAdapter.SCAN_MODE_NONE
            return HostPoweredOff;
        case 21:     // BluetoothAdapter.SCAN_MODE_CONNECTABLE
            return HostConnectable;
        case 23:     // BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE
            return HostDiscoverable;
        default:
            break;
        }
    }

    return HostPoweredOff;
}

QList<QBluetoothHostInfo> QBluetoothLocalDevice::allDevices()
{
    // Android only supports max of one device (so far)
    QList<QBluetoothHostInfo> localDevices;

    QAndroidJniEnvironment env;
    jclass btAdapterClass = env->FindClass("android/bluetooth/BluetoothAdapter");
    if (btAdapterClass == NULL) {
        qCWarning(QT_BT_ANDROID)
            << "Native registration unable to find class android/bluetooth/BluetoothAdapter";
        return localDevices;
    }

    jmethodID getDefaultAdapterID
        = env->GetStaticMethodID(btAdapterClass, "getDefaultAdapter",
                                 "()Landroid/bluetooth/BluetoothAdapter;");
    if (getDefaultAdapterID == NULL) {
        qCWarning(QT_BT_ANDROID)
            << "Native registration unable to get method ID: " \
               "getDefaultAdapter of android/bluetooth/BluetoothAdapter";
        env->DeleteLocalRef(btAdapterClass);
        return localDevices;
    }

    jobject btAdapterObject = env->CallStaticObjectMethod(btAdapterClass, getDefaultAdapterID);
    if (btAdapterObject == NULL) {
        qCWarning(QT_BT_ANDROID) << "Device does not support Bluetooth";
        env->DeleteLocalRef(btAdapterClass);
        return localDevices;
    }

    QAndroidJniObject o(btAdapterObject);
    if (o.isValid()) {
        QBluetoothHostInfo info;
        info.setName(o.callObjectMethod("getName", "()Ljava/lang/String;").toString());
        info.setAddress(QBluetoothAddress(o.callObjectMethod("getAddress",
                                                             "()Ljava/lang/String;").toString()));
        localDevices.append(info);
    }

    env->DeleteLocalRef(btAdapterObject);
    env->DeleteLocalRef(btAdapterClass);

    return localDevices;
}

void QBluetoothLocalDevice::requestPairing(const QBluetoothAddress &address, Pairing pairing)
{
    if (address.isNull()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothLocalDevice::Error,
                                        QBluetoothLocalDevice::PairingError));
        return;
    }

    const Pairing previousPairing = pairingStatus(address);
    Pairing newPairing = pairing;
    if (pairing == AuthorizedPaired) // AuthorizedPaired same as Paired on Android
        newPairing = Paired;

    if (previousPairing == newPairing) {
        QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothAddress, address),
                                  Q_ARG(QBluetoothLocalDevice::Pairing, pairing));
        return;
    }

    // BluetoothDevice::createBond() requires Android API 19
    if (QtAndroidPrivate::androidSdkVersion() < 19 || !d_ptr->adapter()) {
        qCWarning(QT_BT_ANDROID) <<  "Unable to pair: requires Android API 19+";
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothLocalDevice::Error,
                                        QBluetoothLocalDevice::PairingError));
        return;
    }

    QAndroidJniObject inputString = QAndroidJniObject::fromString(address.toString());
    jboolean success = QAndroidJniObject::callStaticMethod<jboolean>(
        "org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver",
        "setPairingMode",
        "(Ljava/lang/String;Z)Z",
        inputString.object<jstring>(),
        newPairing == Paired ? JNI_TRUE : JNI_FALSE);

    if (!success) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothLocalDevice::Error,
                                        QBluetoothLocalDevice::PairingError));
    } else {
        d_ptr->pendingPairings.append(qMakePair(address, newPairing == Paired ? true : false));
    }
}

QBluetoothLocalDevice::Pairing QBluetoothLocalDevice::pairingStatus(
    const QBluetoothAddress &address) const
{
    if (address.isNull() || !d_ptr->adapter())
        return Unpaired;

    QAndroidJniObject inputString = QAndroidJniObject::fromString(address.toString());
    QAndroidJniObject remoteDevice
        = d_ptr->adapter()->callObjectMethod("getRemoteDevice",
                                             "(Ljava/lang/String;)Landroid/bluetooth/BluetoothDevice;",
                                             inputString.object<jstring>());
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return Unpaired;
    }

    jint bondState = remoteDevice.callMethod<jint>("getBondState");
    switch (bondState) {
    case 12: // BluetoothDevice.BOND_BONDED
        return Paired;
    default:
        break;
    }

    return Unpaired;
}

void QBluetoothLocalDevice::pairingConfirmation(bool confirmation)
{
    if (!d_ptr->adapter())
        return;

    bool success = d_ptr->receiver->pairingConfirmation(confirmation);
    if (!success)
        emit error(PairingError);
}

QList<QBluetoothAddress> QBluetoothLocalDevice::connectedDevices() const
{
    /*
     * Android does not have an API to list all connected devices. We have to collect
     * the information based on a few indicators.
     *
     * Primarily we detect connected devices by monitoring connect/disconnect signals.
     * Unfortunately the list may only be complete after very long monitoring time.
     * However there are some Android APIs which provide the list of connected devices
     * for specific Bluetooth profiles. QtBluetoothBroadcastReceiver.getConnectedDevices()
     * returns a few connections of common profiles. The returned list is not complete either
     * but at least it can complement our already detected connections.
     */
    QAndroidJniObject connectedDevices = QAndroidJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver",
        "getConnectedDevices",
        "()[Ljava/lang/String;");

    if (!connectedDevices.isValid())
        return d_ptr->connectedDevices;

    jobjectArray connectedDevicesArray = connectedDevices.object<jobjectArray>();
    if (!connectedDevicesArray)
        return d_ptr->connectedDevices;

    QAndroidJniEnvironment env;
    QList<QBluetoothAddress> knownAddresses = d_ptr->connectedDevices;
    QAndroidJniObject p;

    jint size = env->GetArrayLength(connectedDevicesArray);
    for (int i = 0; i < size; i++) {
        p = env->GetObjectArrayElement(connectedDevicesArray, i);
        QBluetoothAddress address(p.toString());
        if (!address.isNull() && !knownAddresses.contains(address))
            knownAddresses.append(address);
    }

    return knownAddresses;
}

QT_END_NAMESPACE
