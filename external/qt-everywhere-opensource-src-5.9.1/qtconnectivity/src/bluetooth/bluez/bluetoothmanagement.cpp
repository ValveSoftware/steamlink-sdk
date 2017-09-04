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

#include <QtCore/qloggingcategory.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>

#include "bluetoothmanagement_p.h"
#include "bluez_data_p.h"
#include "../qbluetoothsocket_p.h"

#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/capability.h>


QT_BEGIN_NAMESPACE

// Packet data structures for Mgmt API bluez.git/doc/mgmt-api.txt

enum class EventCode {
    DeviceFound =       0x0012,
};

struct MgmtHdr {
    quint16 cmdCode;
    quint16 controllerIndex;
    quint16 length;
} __attribute__((packed));

struct MgmtEventDeviceFound {
    bdaddr_t bdaddr;
    quint8 type;
    quint8 rssi;
    quint32 flags;
    quint16 eirLength;
    quint8 eirData[0];
}  __attribute__((packed));


/*
 * This class encapsulates access to the Bluetooth Management API as introduced by
 * Linux kernel 3.4. Some Bluetooth information is not exposed via the usual DBus
 * API (e.g. the random/public address type info). In those cases we have to fall back
 * to this mgmt API.
 *
 * Note that opening such a Bluetooth mgmt socket requires CAP_NET_ADMIN (root) capability.
 *
 * Documentation can be found in bluez-git/doc/mgmt-api.txt
 */

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

// These structs and defines come straight from linux/capability.h.
// To avoid missing definitions we re-define them if not existing.
// In addition, we don't want to pull in a libcap2 dependency
struct capHdr {
    quint32 version;
    int pid;
};

struct capData {
    quint32 effective;
    quint32 permitted;
    quint32 inheritable;
};

#ifndef _LINUX_CAPABILITY_VERSION_3
#define _LINUX_CAPABILITY_VERSION_3 0x20080522
#endif

#ifndef _LINUX_CAPABILITY_U32S_3
#define _LINUX_CAPABILITY_U32S_3 2
#endif

#ifndef CAP_NET_ADMIN
#define CAP_NET_ADMIN 12
#endif

#ifndef CAP_TO_INDEX
#define CAP_TO_INDEX(x)     ((x) >> 5)        /* 1 << 5 == bits in __u32 */
#endif

#ifndef CAP_TO_MASK
#define CAP_TO_MASK(x)      (1 << ((x) & 31)) /* mask for indexed __u32 */
#endif

const int msecInADay = 1000*60*60*24;

inline uint qHash(const QBluetoothAddress& address)
{
    return qHash(address.toUInt64());
}

static int sysCallCapGet(capHdr *header, capData *data)
{
    return syscall(__NR_capget, header, data);
}

/*!
 * Checks that the current process has the effective CAP_NET_ADMIN permission.
 */
static bool hasBtMgmtPermission()
{
    // We only care for cap version 3 introduced by kernel 2.6.26
    // because the new BlueZ management API only exists since kernel 3.4.

    struct capHdr header = {};
    struct capData data[_LINUX_CAPABILITY_U32S_3] = {{}};
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = getpid();

    if (sysCallCapGet(&header, data) < 0) {
        qCWarning(QT_BT_BLUEZ, "BluetoothManangement: getCap failed with %s",
                               qPrintable(qt_error_string(errno)));
        return false;
    }

    return (data[CAP_TO_INDEX(CAP_NET_ADMIN)].effective & CAP_TO_MASK(CAP_NET_ADMIN));
}

BluetoothManagement::BluetoothManagement(QObject *parent) : QObject(parent)
{
    bool hasPermission = hasBtMgmtPermission();
    if (!hasPermission) {
        qCInfo(QT_BT_BLUEZ, "Missing CAP_NET_ADMIN permission. Cannot determine whether "
                             "a found address is of random or public type.");
        return;
    }

    sockaddr_hci hciAddr;

    fd = ::socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, BTPROTO_HCI);
    if (fd < 0) {
        qCWarning(QT_BT_BLUEZ, "Cannot open Bluetooth Management socket: %s",
                               qPrintable(qt_error_string(errno)));
        return;
    }

    memset(&hciAddr, 0, sizeof(hciAddr));
    hciAddr.hci_dev = HCI_DEV_NONE;
    hciAddr.hci_channel = HCI_CHANNEL_CONTROL;
    hciAddr.hci_family = AF_BLUETOOTH;

    if (::bind(fd, (struct sockaddr *)(&hciAddr), sizeof(hciAddr)) < 0) {
        qCWarning(QT_BT_BLUEZ, "Cannot bind Bluetooth Management socket: %s",
                               qPrintable(qt_error_string(errno)));
        ::close(fd);
        fd = -1;
        return;
    }

    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &BluetoothManagement::_q_readNotifier);

    // ensure cache is regularly cleaned (once every 24h)
    QTimer* timer = new QTimer(this);
    timer->setInterval(msecInADay);
    timer->setTimerType(Qt::VeryCoarseTimer);
    connect(timer, &QTimer::timeout, this, &BluetoothManagement::cleanupOldAddressFlags);
    timer->start();
}

Q_GLOBAL_STATIC(BluetoothManagement, bluetoothKernelManager)

BluetoothManagement *BluetoothManagement::instance()
{
    return bluetoothKernelManager();
}

void BluetoothManagement::_q_readNotifier()
{
    char *dst = buffer.reserve(QPRIVATELINEARBUFFER_BUFFERSIZE);
    int readCount = ::read(fd, dst, QPRIVATELINEARBUFFER_BUFFERSIZE);
    buffer.chop(QPRIVATELINEARBUFFER_BUFFERSIZE - (readCount < 0 ? 0 : readCount));
    if (readCount < 0) {
        qCWarning(QT_BT_BLUEZ, "Management Control read error %s", qPrintable(qt_error_string(errno)));
        return;
    }

    // do we have at least one complete mgmt header?
    if ((uint)buffer.size() < sizeof(MgmtHdr))
        return;

    QByteArray data = buffer.readAll();

    while (true) {
        if ((uint)data.size() < sizeof(MgmtHdr))
            break;

        const MgmtHdr *hdr = reinterpret_cast<const MgmtHdr*>(data.constData());
        const int nextPackageSize = qFromLittleEndian(hdr->length) + sizeof(MgmtHdr);
        const int remainingPackageSize = data.length() - nextPackageSize;

        if (data.length() < nextPackageSize)
            break; // not a complete event header -> wait for next notifier

        switch (static_cast<EventCode>(qFromLittleEndian(hdr->cmdCode))) {
        case EventCode::DeviceFound:
        {
            const MgmtEventDeviceFound *event = reinterpret_cast<const MgmtEventDeviceFound*>
                                                   (data.constData() + sizeof(MgmtHdr));

            if (event->type == BDADDR_LE_RANDOM) {
                const bdaddr_t address = event->bdaddr;
                quint64 bdaddr;

                convertAddress(address.b, &bdaddr);
                const QBluetoothAddress qtAddress(bdaddr);
                qCDebug(QT_BT_BLUEZ) << "BluetoothManagement: found random device"
                                     << qtAddress;
                processRandomAddressFlagInformation(qtAddress);
            }

            break;
        }
        default:
            qCDebug(QT_BT_BLUEZ) << "BluetoothManagement: Ignored event:"
                                 << hex << qFromLittleEndian(hdr->cmdCode);
            break;
        }

        if (data.length() > nextPackageSize)
            data = data.right(remainingPackageSize);
        else
            data.clear();

        if (data.isEmpty())
            break;
    }

    if (!data.isEmpty())
        buffer.ungetBlock(data.constData(), data.size());
}

void BluetoothManagement::processRandomAddressFlagInformation(const QBluetoothAddress &address)
{
    // insert or update
    QMutexLocker locker(&accessLock);
    privateFlagAddresses[address] = QDateTime::currentDateTimeUtc();
}

/*
 * Ensure that private address cache is not older than 24h.
 */
void BluetoothManagement::cleanupOldAddressFlags()
{
    const auto cutOffTime = QDateTime::currentDateTimeUtc().addDays(-1);

    QMutexLocker locker(&accessLock);

    auto i = privateFlagAddresses.begin();
    while (i != privateFlagAddresses.end()) {
        if (i.value() < cutOffTime)
            i = privateFlagAddresses.erase(i);
        else
            i++;
    }
}

bool BluetoothManagement::isAddressRandom(const QBluetoothAddress &address) const
{
    if (fd == -1 || address.isNull())
        return false;

    QMutexLocker locker(&accessLock);
    return privateFlagAddresses.contains(address);
}

bool BluetoothManagement::isMonitoringEnabled() const
{
    return (fd == -1) ? false : true;
}


QT_END_NAMESPACE
