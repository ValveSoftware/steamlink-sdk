/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 Javier S. Pedro <maemo@javispedro.com>
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

#include "hcimanager_p.h"

#include "qbluetoothsocket_p.h"

#include <QtCore/QLoggingCategory>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define HCIGETCONNLIST  _IOR('H', 212, int)
#define HCIGETDEVINFO   _IOR('H', 211, int)
#define HCIGETDEVLIST   _IOR('H', 210, int)

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

HciManager::HciManager(const QBluetoothAddress& deviceAdapter, QObject *parent) :
    QObject(parent), hciSocket(-1), hciDev(-1), notifier(0)
{
    hciSocket = ::socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (hciSocket < 0) {
        qCWarning(QT_BT_BLUEZ) << "Cannot open HCI socket";
        return; //TODO error report
    }

    hciDev = hciForAddress(deviceAdapter);
    if (hciDev < 0) {
        qCWarning(QT_BT_BLUEZ) << "Cannot find hci dev for" << deviceAdapter.toString();
        close(hciSocket);
        hciSocket = -1;
        return;
    }

    struct sockaddr_hci addr;

    memset(&addr, 0, sizeof(struct sockaddr_hci));
    addr.hci_dev = hciDev;
    addr.hci_family = AF_BLUETOOTH;

    if (::bind(hciSocket, (struct sockaddr *) (&addr), sizeof(addr)) < 0) {
        qCWarning(QT_BT_BLUEZ) << "HCI bind failed:" << strerror(errno);
        close(hciSocket);
        hciSocket = hciDev = -1;
        return;
    }

    notifier = new QSocketNotifier(hciSocket, QSocketNotifier::Read, this);
    connect(notifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));

}

HciManager::~HciManager()
{
    if (hciSocket >= 0)
        ::close(hciSocket);

}

bool HciManager::isValid() const
{
    if (hciSocket && hciDev >= 0)
        return true;
    return false;
}

int HciManager::hciForAddress(const QBluetoothAddress &deviceAdapter)
{
    if (hciSocket < 0)
        return -1;

    bdaddr_t adapter;
    convertAddress(deviceAdapter.toUInt64(), adapter.b);

    struct hci_dev_req *devRequest = 0;
    struct hci_dev_list_req *devRequestList = 0;
    struct hci_dev_info devInfo;
    const int devListSize = sizeof(struct hci_dev_list_req)
                        + HCI_MAX_DEV * sizeof(struct hci_dev_req);

    devRequestList = (hci_dev_list_req *) malloc(devListSize);
    if (!devRequestList)
        return -1;

    QScopedPointer<hci_dev_list_req, QScopedPointerPodDeleter> p(devRequestList);

    memset(p.data(), 0, devListSize);
    p->dev_num = HCI_MAX_DEV;
    devRequest = p->dev_req;

    if (ioctl(hciSocket, HCIGETDEVLIST, devRequestList) < 0)
        return -1;

    for (int i = 0; i < devRequestList->dev_num; i++) {
        devInfo.dev_id = (devRequest+i)->dev_id;
        if (ioctl(hciSocket, HCIGETDEVINFO, &devInfo) < 0) {
            continue;
        }

        int result = memcmp(&adapter, &devInfo.bdaddr, sizeof(bdaddr_t));
        if (result == 0 || deviceAdapter.isNull()) // addresses match
            return devRequest->dev_id;
    }

    return -1;
}

/*
 * Returns true if \a event was successfully enabled
 */
bool HciManager::monitorEvent(HciManager::HciEvent event)
{
    if (!isValid())
        return false;

    // this event is already enabled
    // TODO runningEvents does not seem to be used
    if (runningEvents.contains(event))
        return true;

    hci_filter filter;
    socklen_t length = sizeof(hci_filter);
    if (getsockopt(hciSocket, SOL_HCI, HCI_FILTER, &filter, &length) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Cannot retrieve HCI filter settings";
        return false;
    }

    hci_filter_set_ptype(HCI_EVENT_PKT, &filter);
    hci_filter_set_event(event, &filter);
    //hci_filter_all_events(&filter);

    if (setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &filter, sizeof(hci_filter)) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Could not set HCI socket options:" << strerror(errno);
        return false;
    }

    return true;
}

/*
 * Unsubscribe from all events
 */
void HciManager::stopEvents()
{
    if (!isValid())
        return;

    hci_filter filter;
    hci_filter_clear(&filter);

    if (setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &filter, sizeof(hci_filter)) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Could not clear HCI socket options:" << strerror(errno);
        return;
    }

    runningEvents.clear();
}

QBluetoothAddress HciManager::addressForConnectionHandle(quint16 handle) const
{
    if (!isValid())
        return QBluetoothAddress();

    hci_conn_info *info;
    hci_conn_list_req *infoList;

    const int maxNoOfConnections = 20;
    infoList = (hci_conn_list_req *)
            malloc(sizeof(hci_conn_list_req) + maxNoOfConnections * sizeof(hci_conn_info));

    if (!infoList)
        return QBluetoothAddress();

    QScopedPointer<hci_conn_list_req, QScopedPointerPodDeleter> p(infoList);
    p->conn_num = maxNoOfConnections;
    p->dev_id = hciDev;
    info = p->conn_info;

    if (ioctl(hciSocket, HCIGETCONNLIST, (void *) infoList) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Cannot retrieve connection list";
        return QBluetoothAddress();
    }

    for (int i = 0; i < infoList->conn_num; i++) {
        if (info[i].handle == handle) {
            quint64 converted;
            convertAddress(info[i].bdaddr.b, converted);

            return QBluetoothAddress(converted);
        }
    }

    return QBluetoothAddress();
}

/*!
 * Process all incoming HCI events. Function cannot process anything else but events.
 */
void HciManager::_q_readNotify()
{

    unsigned char buffer[HCI_MAX_EVENT_SIZE];
    int size;

    size = ::read(hciSocket, buffer, sizeof(buffer));
    if (size < 0) {
        if (errno != EAGAIN && errno != EINTR)
            qCWarning(QT_BT_BLUEZ) << "Failed reading HCI events:" << qt_error_string(errno);

        return;
    }

    const unsigned char *data = buffer;

    // Not interested in anything but valid HCI events
    if ((size < HCI_EVENT_HDR_SIZE + 1) || buffer[0] != HCI_EVENT_PKT)
        return;

    hci_event_hdr *header = (hci_event_hdr *)(&buffer[1]);

    size = size - HCI_EVENT_HDR_SIZE - 1;
    data = data + HCI_EVENT_HDR_SIZE + 1;

    if (header->plen != size) {
        qCWarning(QT_BT_BLUEZ) << "Invalid HCI event packet size";
        return;
    }

    qCDebug(QT_BT_BLUEZ) << "HCI event triggered, type:" << hex << header->evt;

    switch (header->evt) {
    case EVT_ENCRYPT_CHANGE:
    {
        const evt_encrypt_change *event = (evt_encrypt_change *) data;
        qCDebug(QT_BT_BLUEZ) << "HCI Encrypt change, status:"
                             << (event->status == 0 ? "Success" : "Failed")
                             << "handle:" << hex << event->handle
                             << "encrypt:" << event->encrypt;

        QBluetoothAddress remoteDevice = addressForConnectionHandle(event->handle);
        if (!remoteDevice.isNull())
            emit encryptionChangedEvent(remoteDevice, event->status == 0);
    }
        break;
    default:
        break;
    }
}


QT_END_NAMESPACE
