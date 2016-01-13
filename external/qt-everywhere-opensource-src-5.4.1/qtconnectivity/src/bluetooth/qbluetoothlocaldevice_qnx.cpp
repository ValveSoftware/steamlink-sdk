/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
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

#include "qbluetoothlocaldevice.h"
#include "qbluetoothaddress.h"
#include "qbluetoothlocaldevice_p.h"
#include <sys/pps.h>
#include "qnx/ppshelpers_p.h"
#include <QDir>
#include <QtCore/private/qcore_unix_p.h>

QT_BEGIN_NAMESPACE

QBluetoothLocalDevice::QBluetoothLocalDevice(QObject *parent) :
    QObject(parent)
{
    this->d_ptr = new QBluetoothLocalDevicePrivate(this);
    this->d_ptr->isValidDevice = true; // assume single local device on QNX
}

QBluetoothLocalDevice::QBluetoothLocalDevice(const QBluetoothAddress &address, QObject *parent) :
    QObject(parent)
{
    this->d_ptr = new QBluetoothLocalDevicePrivate(this);

    // works since we assume a single local device on QNX
    this->d_ptr->isValidDevice = (QBluetoothLocalDevicePrivate::address() == address
                                  || address == QBluetoothAddress());
}

QString QBluetoothLocalDevice::name() const
{
    if (this->d_ptr->isValid())
        return this->d_ptr->name();
    return QString();
}

QBluetoothAddress QBluetoothLocalDevice::address() const
{
    if (this->d_ptr->isValid())
        return this->d_ptr->address();
    return QBluetoothAddress();
}

void QBluetoothLocalDevice::powerOn()
{
    if (hostMode() == QBluetoothLocalDevice::HostPoweredOff)
        this->d_ptr->powerOn();
}

void QBluetoothLocalDevice::setHostMode(QBluetoothLocalDevice::HostMode mode)
{
    this->d_ptr->setHostMode(mode);
}

QBluetoothLocalDevice::HostMode QBluetoothLocalDevice::hostMode() const
{
    return this->d_ptr->hostMode();
}

QList<QBluetoothAddress> QBluetoothLocalDevice::connectedDevices() const
{
    QList<QBluetoothAddress> devices;
    QDir bluetoothDevices(QStringLiteral("/pps/services/bluetooth/remote_devices/"));
    QStringList allFiles = bluetoothDevices.entryList(QDir::NoDotAndDotDot| QDir::Files);
    for (int i = 0; i < allFiles.size(); i++) {
        qCDebug(QT_BT_QNX) << allFiles.at(i);
        int fileId;
        const char *filePath = QByteArray("/pps/services/bluetooth/remote_devices/").append(allFiles.at(
                                                                                                i).toUtf8().constData())
                               .constData();
        if ((fileId = qt_safe_open(filePath, O_RDONLY)) == -1) {
            qCWarning(QT_BT_QNX) << "Failed to open remote device file";
        } else {
            pps_decoder_t ppsDecoder;
            pps_decoder_initialize(&ppsDecoder, 0);

            QBluetoothAddress deviceAddr;
            QString deviceName;

            if (!ppsReadRemoteDevice(fileId, &ppsDecoder, &deviceAddr, &deviceName)) {
                pps_decoder_cleanup(&ppsDecoder);
                qDebug() << "Failed to open remote device file";
            }

            bool connectedDevice = false;
            int a = pps_decoder_get_bool(&ppsDecoder, "acl_connected", &connectedDevice);
            if (a == PPS_DECODER_OK) {
                if (connectedDevice)
                    devices.append(deviceAddr);
            } else if (a == PPS_DECODER_BAD_TYPE) {
                qCDebug(QT_BT_QNX) << "Type missmatch";
            } else {
                qCDebug(QT_BT_QNX) << "An unknown error occurred while checking connected status.";
            }
            pps_decoder_cleanup(&ppsDecoder);
        }
    }

    return devices;
}

QList<QBluetoothHostInfo> QBluetoothLocalDevice::allDevices()
{
    // We only have one device
    QList<QBluetoothHostInfo> localDevices;
    QBluetoothHostInfo hostInfo;
    hostInfo.setName(QBluetoothLocalDevicePrivate::name());
    hostInfo.setAddress(QBluetoothLocalDevicePrivate::address());
    localDevices.append(hostInfo);
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

    const Pairing current_pairing = pairingStatus(address);
    if (current_pairing == pairing) {
        QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothAddress, address),
                                  Q_ARG(QBluetoothLocalDevice::Pairing, pairing));
        return;
    }
    d_ptr->requestPairing(address, pairing);
}

QBluetoothLocalDevice::Pairing QBluetoothLocalDevice::pairingStatus(
    const QBluetoothAddress &address) const
{
    if (!isValid())
        return Unpaired;
    bool paired = false;
    bool btle = false; // Bluetooth Low Energy devices
    QByteArray qnxPath("/pps/services/bluetooth/remote_devices/");
    qnxPath.append(address.toString().toUtf8());
    int m_rdfd;
    if ((m_rdfd = qt_safe_open(qnxPath.constData(), O_RDONLY)) == -1) {
        btle = true;
        qnxPath.append("-00");
        if ((m_rdfd = qt_safe_open(qnxPath.constData(), O_RDONLY)) == -1) {
            qnxPath.replace((qnxPath.length()-3), 3, "-01");
            if ((m_rdfd = qt_safe_open(qnxPath.constData(), O_RDONLY)) == -1)
                return Unpaired;
        }
    }

    pps_decoder_t ppsDecoder;
    pps_decoder_initialize(&ppsDecoder, NULL);

    QBluetoothAddress deviceAddr;
    QString deviceName;

    if (!ppsReadRemoteDevice(m_rdfd, &ppsDecoder, &deviceAddr, &deviceName))
        return Unpaired;
    bool known = false;
    // Paired BTLE devices have only known field set to true.
    if (btle)
        pps_decoder_get_bool(&ppsDecoder, "known", &known);
    pps_decoder_get_bool(&ppsDecoder, "paired", &paired);
    pps_decoder_cleanup(&ppsDecoder);

    if (paired)
        return Paired;
    else if (btle && known)
        return Paired;
    else
        return Unpaired;
}

void QBluetoothLocalDevice::pairingConfirmation(bool confirmation)
{
    Q_UNUSED(confirmation);
}

QBluetoothLocalDevicePrivate::QBluetoothLocalDevicePrivate(QBluetoothLocalDevice *q) :
    q_ptr(q)
{
    ppsRegisterControl();
    ppsRegisterForEvent(QStringLiteral("access_changed"), this);
    ppsRegisterForEvent(QStringLiteral("pairing_complete"), this);
    ppsRegisterForEvent(QStringLiteral("device_deleted"), this);
    ppsRegisterForEvent(QStringLiteral("radio_shutdown"), this);
}

QBluetoothLocalDevicePrivate::~QBluetoothLocalDevicePrivate()
{
    ppsUnregisterControl(this);
    ppsUnregisterForEvent(QStringLiteral("access_changed"), this);
    ppsUnregisterForEvent(QStringLiteral("pairing_complete"), this);
    ppsUnregisterForEvent(QStringLiteral("device_deleted"), this);
    ppsUnregisterForEvent(QStringLiteral("radio_shutdown"), this);
}

bool QBluetoothLocalDevicePrivate::isValid() const
{
    return isValidDevice;
}

QBluetoothAddress QBluetoothLocalDevicePrivate::address()
{
    return QBluetoothAddress(ppsReadSetting("btaddr").toString());
}

QString QBluetoothLocalDevicePrivate::name()
{
    return ppsReadSetting("name").toString();
}

void QBluetoothLocalDevicePrivate::powerOn()
{
    if (isValid())
        ppsSendControlMessage("radio_init", this);
}

void QBluetoothLocalDevicePrivate::powerOff()
{
    if (isValid())
        ppsSendControlMessage("radio_shutdown", this);
}

void QBluetoothLocalDevicePrivate::setHostMode(QBluetoothLocalDevice::HostMode mode)
{
    if (!isValid())
        return;

    QBluetoothLocalDevice::HostMode currentHostMode = hostMode();
    if (currentHostMode == mode)
        return;
    // If the device is in PowerOff state and the profile is changed then the power has to be turned on
    if (currentHostMode == QBluetoothLocalDevice::HostPoweredOff) {
        qCDebug(QT_BT_QNX) << "Powering on";
        powerOn();
    }

    if (mode == QBluetoothLocalDevice::HostPoweredOff)
        powerOff();
    else if (mode == QBluetoothLocalDevice::HostDiscoverable)   // General discoverable and connectable.
        setAccess(1);
    else if (mode == QBluetoothLocalDevice::HostConnectable)   // Connectable but not discoverable.
        setAccess(3);
    else if (mode == QBluetoothLocalDevice::HostDiscoverableLimitedInquiry)   // Limited discoverable and connectable.
        setAccess(2);
}

void QBluetoothLocalDevicePrivate::requestPairing(const QBluetoothAddress &address,
                                                  QBluetoothLocalDevice::Pairing pairing)
{
    if (pairing == QBluetoothLocalDevice::Paired
        || pairing == QBluetoothLocalDevice::AuthorizedPaired) {
        ppsSendControlMessage("initiate_pairing",
                              QStringLiteral("{\"addr\":\"%1\"}").arg(address.toString()),
                              this);
    } else {
        ppsSendControlMessage("remove_device",
                              QStringLiteral("{\"addr\":\"%1\"}").arg(address.toString()),
                              this);
    }
}

QBluetoothLocalDevice::HostMode QBluetoothLocalDevicePrivate::hostMode() const
{
    if (!isValid())
        return QBluetoothLocalDevice::HostPoweredOff;

    if (!ppsReadSetting("enabled").toBool())
        return QBluetoothLocalDevice::HostPoweredOff;

    int hostMode = ppsReadSetting("accessibility").toInt();

    if (hostMode == 1) // General discoverable and connectable.
        return QBluetoothLocalDevice::HostDiscoverable;
    else if (hostMode == 3)  // Connectable but not discoverable.
        return QBluetoothLocalDevice::HostConnectable;
    else if (hostMode == 2)  // Limited discoverable and connectable.
        return QBluetoothLocalDevice::HostDiscoverableLimitedInquiry;
    else
        return QBluetoothLocalDevice::HostPoweredOff;
}

extern int __newHostMode;

void QBluetoothLocalDevicePrivate::setAccess(int access)
{
    if (!ppsReadSetting("enabled").toBool())   // We cannot set the host mode until BT is fully powered up
        __newHostMode = access;
    else
        ppsSendControlMessage("set_access", QStringLiteral("{\"access\":%1}").arg(access), 0);
}

void QBluetoothLocalDevicePrivate::connectedDevices()
{
    QList<QBluetoothAddress> devices = q_ptr->connectedDevices();
    for (int i = 0; i < devices.size(); i++) {
        if (!connectedDevicesSet.contains(devices.at(i))) {
            QBluetoothAddress addr = devices.at(i);
            connectedDevicesSet.append(addr);
            emit q_ptr->deviceConnected(devices.at(i));
        }
    }

    for (int i = 0; i < connectedDevicesSet.size(); i++) {
        if (!devices.contains(connectedDevicesSet.at(i))) {
            QBluetoothAddress addr = connectedDevicesSet.at(i);
            emit q_ptr->deviceDisconnected(addr);
            connectedDevicesSet.removeOne(addr);
        }
    }
}

void QBluetoothLocalDevicePrivate::controlReply(ppsResult result)
{
    qCDebug(QT_BT_QNX) << Q_FUNC_INFO << result.msg << result.dat;
    if (!result.errorMsg.isEmpty()) {
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << result.errorMsg;
        if (result.msg == QStringLiteral("initiate_pairing"))
            q_ptr->error(QBluetoothLocalDevice::PairingError);
        else
            q_ptr->error(QBluetoothLocalDevice::UnknownError);
    }
}

void QBluetoothLocalDevicePrivate::controlEvent(ppsResult result)
{
    qCDebug(QT_BT_QNX) << Q_FUNC_INFO << "Control Event" << result.msg;
    if (result.msg == QStringLiteral("access_changed")) {
        if (__newHostMode == -1 && result.dat.size() > 1
            && result.dat.first() == QStringLiteral("level")) {
            QBluetoothLocalDevice::HostMode newHostMode = hostMode();
            qCDebug(QT_BT_QNX) << "New Host mode" << newHostMode;
            connectedDevices();
            emit q_ptr->hostModeStateChanged(newHostMode);
        }
    } else if (result.msg == QStringLiteral("pairing_complete")) {
        qCDebug(QT_BT_QNX) << "pairing completed";
        if (result.dat.contains(QStringLiteral("addr"))) {
            const QBluetoothAddress address = QBluetoothAddress(
                result.dat.at(result.dat.indexOf(QStringLiteral("addr")) + 1));

            QBluetoothLocalDevice::Pairing pairingStatus = QBluetoothLocalDevice::Paired;

            if (result.dat.contains(QStringLiteral("trusted"))
                && result.dat.at(result.dat.indexOf(QStringLiteral("trusted")) + 1)
                == QStringLiteral("true")) {
                pairingStatus = QBluetoothLocalDevice::AuthorizedPaired;
            }
            qCDebug(QT_BT_QNX) << "pairing completed" << address.toString();
            emit q_ptr->pairingFinished(address, pairingStatus);
        }
    } else if (result.msg == QStringLiteral("device_deleted")) {
        qCDebug(QT_BT_QNX) << "device deleted";
        if (result.dat.contains(QStringLiteral("addr"))) {
            const QBluetoothAddress address = QBluetoothAddress(
                result.dat.at(result.dat.indexOf(QStringLiteral("addr")) + 1));
            emit q_ptr->pairingFinished(address, QBluetoothLocalDevice::Unpaired);
        }
    } else if (result.msg == QStringLiteral("radio_shutdown")) {
        qCDebug(QT_BT_QNX) << "radio shutdown";
        emit q_ptr->hostModeStateChanged(QBluetoothLocalDevice::HostPoweredOff);
    }
}

QT_END_NAMESPACE
