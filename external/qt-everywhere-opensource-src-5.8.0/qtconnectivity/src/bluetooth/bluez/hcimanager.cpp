/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Javier S. Pedro <maemo@javispedro.com>
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

#include "hcimanager_p.h"

#include "qbluetoothsocket_p.h"
#include "qlowenergyconnectionparameters.h"

#include <QtCore/qloggingcategory.h>

#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
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
            return devInfo.dev_id;
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

bool HciManager::monitorAclPackets()
{
    if (!isValid())
        return false;

    hci_filter filter;
    socklen_t length = sizeof(hci_filter);
    if (getsockopt(hciSocket, SOL_HCI, HCI_FILTER, &filter, &length) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Cannot retrieve HCI filter settings";
        return false;
    }

    hci_filter_set_ptype(HCI_ACL_PKT, &filter);
    hci_filter_all_events(&filter);

    if (setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &filter, sizeof(hci_filter)) < 0) {
        qCWarning(QT_BT_BLUEZ) << "Could not set HCI socket options:" << strerror(errno);
        return false;
    }

    return true;
}

bool HciManager::sendCommand(OpCodeGroupField ogf, OpCodeCommandField ocf, const QByteArray &parameters)
{
    qCDebug(QT_BT_BLUEZ) << "sending command; ogf:" << ogf << "ocf:" << ocf;
    quint8 packetType = HCI_COMMAND_PKT;
    hci_command_hdr command = {
        opCodePack(ogf, ocf),
        static_cast<uint8_t>(parameters.count())
    };
    static_assert(sizeof command == 3, "unexpected struct size");
    struct iovec iv[3];
    iv[0].iov_base = &packetType;
    iv[0].iov_len  = 1;
    iv[1].iov_base = &command;
    iv[1].iov_len  = sizeof command;
    int ivn = 2;
    if (!parameters.isEmpty()) {
        iv[2].iov_base = const_cast<char *>(parameters.constData()); // const_cast is safe, since iov_base will not get modified.
        iv[2].iov_len  = parameters.count();
        ++ivn;
    }
    while (writev(hciSocket, iv, ivn) < 0) {
        if (errno == EAGAIN || errno == EINTR)
            continue;
        qCDebug(QT_BT_BLUEZ()) << "hci command failure:" << strerror(errno);
        return false;
    }
    qCDebug(QT_BT_BLUEZ) << "command sent successfully";
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
        if (info[i].handle == handle)
            return QBluetoothAddress(convertAddress(info[i].bdaddr.b));
    }

    return QBluetoothAddress();
}

quint16 forceIntervalIntoRange(double connectionInterval)
{
    return qMin<double>(qMax<double>(7.5, connectionInterval), 4000) / 1.25;
}

struct ConnectionUpdateData {
    quint16 minInterval;
    quint16 maxInterval;
    quint16 slaveLatency;
    quint16 timeout;
};
ConnectionUpdateData connectionUpdateData(const QLowEnergyConnectionParameters &params)
{
    ConnectionUpdateData data;
    const quint16 minInterval = forceIntervalIntoRange(params.minimumInterval());
    const quint16 maxInterval = forceIntervalIntoRange(params.maximumInterval());
    data.minInterval = qToLittleEndian(minInterval);
    data.maxInterval = qToLittleEndian(maxInterval);
    const quint16 latency = qMax<quint16>(0, qMin<quint16>(params.latency(), 499));
    data.slaveLatency = qToLittleEndian(latency);
    const quint16 timeout
            = qMax<quint16>(100, qMin<quint16>(32000, params.supervisionTimeout())) / 10;
    data.timeout = qToLittleEndian(timeout);
    return data;
}

bool HciManager::sendConnectionUpdateCommand(quint16 handle,
                                             const QLowEnergyConnectionParameters &params)
{
    struct CommandParams {
        quint16 handle;
        ConnectionUpdateData data;
        quint16 minCeLength;
        quint16 maxCeLength;
    } commandParams;
    commandParams.handle = qToLittleEndian(handle);
    commandParams.data = connectionUpdateData(params);
    commandParams.minCeLength = 0;
    commandParams.maxCeLength = qToLittleEndian(quint16(0xffff));
    const QByteArray data = QByteArray::fromRawData(reinterpret_cast<char *>(&commandParams),
                                                    sizeof commandParams);
    return sendCommand(OgfLinkControl, OcfLeConnectionUpdate, data);
}

bool HciManager::sendConnectionParameterUpdateRequest(quint16 handle,
                                                      const QLowEnergyConnectionParameters &params)
{
    ConnectionUpdateData connUpdateData = connectionUpdateData(params);

    // Vol 3, part A, 4
    struct SignalingPacket {
        quint8 code;
        quint8 identifier;
        quint16 length;
    } signalingPacket;
    signalingPacket.code = 0x12;
    signalingPacket.identifier = ++sigPacketIdentifier;
    const quint16 sigPacketLen = sizeof connUpdateData;
    signalingPacket.length = qToLittleEndian(sigPacketLen);

    L2CapHeader l2CapHeader;
    const quint16 l2CapHeaderLen = sizeof signalingPacket + sigPacketLen;
    l2CapHeader.length = qToLittleEndian(l2CapHeaderLen);
    l2CapHeader.channelId = qToLittleEndian(quint16(SIGNALING_CHANNEL_ID));

    // Vol 2, part E, 5.4.2
    AclData aclData;
    aclData.handle = qToLittleEndian(handle); // Works because the next two values are zero.
    aclData.pbFlag = 0;
    aclData.bcFlag = 0;
    aclData.dataLen = qToLittleEndian(quint16(sizeof l2CapHeader + l2CapHeaderLen));

    struct iovec iv[5];
    quint8 packetType = HCI_ACL_PKT;
    iv[0].iov_base = &packetType;
    iv[0].iov_len  = 1;
    iv[1].iov_base = &aclData;
    iv[1].iov_len  = sizeof aclData;
    iv[2].iov_base = &l2CapHeader;
    iv[2].iov_len = sizeof l2CapHeader;
    iv[3].iov_base = &signalingPacket;
    iv[3].iov_len = sizeof signalingPacket;
    iv[4].iov_base = &connUpdateData;
    iv[4].iov_len = sizeof connUpdateData;
    while (writev(hciSocket, iv, sizeof iv / sizeof *iv) < 0) {
        if (errno == EAGAIN || errno == EINTR)
            continue;
        qCDebug(QT_BT_BLUEZ()) << "failure writing HCI ACL packet:" << strerror(errno);
        return false;
    }
    qCDebug(QT_BT_BLUEZ) << "Connection Update Request packet sent successfully";
    return true;
}

/*!
 * Process all incoming HCI events. Function cannot process anything else but events.
 */
void HciManager::_q_readNotify()
{
    unsigned char buffer[qMax<int>(HCI_MAX_EVENT_SIZE, sizeof(AclData))];
    int size;

    size = ::read(hciSocket, buffer, sizeof(buffer));
    if (size < 0) {
        if (errno != EAGAIN && errno != EINTR)
            qCWarning(QT_BT_BLUEZ) << "Failed reading HCI events:" << qt_error_string(errno);

        return;
    }

    switch (buffer[0]) {
    case HCI_EVENT_PKT:
        handleHciEventPacket(buffer + 1, size - 1);
        break;
    case HCI_ACL_PKT:
        handleHciAclPacket(buffer + 1, size - 1);
        break;
    default:
        qCWarning(QT_BT_BLUEZ) << "Ignoring unexpected HCI packet type" << buffer[0];
    }
}

void HciManager::handleHciEventPacket(const quint8 *data, int size)
{
    if (size < HCI_EVENT_HDR_SIZE) {
        qCWarning(QT_BT_BLUEZ) << "Unexpected HCI event packet size:" << size;
        return;
    }

    hci_event_hdr *header = (hci_event_hdr *) data;

    size -= HCI_EVENT_HDR_SIZE;
    data += HCI_EVENT_HDR_SIZE;

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
    case EVT_CMD_COMPLETE: {
        auto * const event = reinterpret_cast<const evt_cmd_complete *>(data);
        static_assert(sizeof *event == 3, "unexpected struct size");

        // There is always a status byte right after the generic structure.
        Q_ASSERT(size > static_cast<int>(sizeof *event));
        const quint8 status = data[sizeof *event];
        const auto additionalData = QByteArray(reinterpret_cast<const char *>(data)
                                               + sizeof *event + 1, size - sizeof *event - 1);
        emit commandCompleted(event->opcode, status, additionalData);
    }
        break;
    case LeMetaEvent:
        handleLeMetaEvent(data);
        break;
    default:
        break;
    }

}

void HciManager::handleHciAclPacket(const quint8 *data, int size)
{
    if (size < int(sizeof(AclData))) {
        qCWarning(QT_BT_BLUEZ) << "Unexpected HCI ACL packet size";
        return;
    }

    quint16 rawAclData[sizeof(AclData) / sizeof(quint16)];
    rawAclData[0] = bt_get_le16(data);
    rawAclData[1] = bt_get_le16(data + sizeof(quint16));
    const AclData *aclData = reinterpret_cast<AclData *>(rawAclData);
    data += sizeof *aclData;
    size -= sizeof *aclData;

    // Consider only directed, complete messages.
    if ((aclData->pbFlag != 0 && aclData->pbFlag != 2) || aclData->bcFlag != 0)
        return;

    if (size < aclData->dataLen) {
        qCWarning(QT_BT_BLUEZ) << "HCI ACL packet data size" << size
                               << "is smaller than specified size" << aclData->dataLen;
        return;
    }

//    qCDebug(QT_BT_BLUEZ) << "handle:" << aclData->handle << "PB:" << aclData->pbFlag
//                         << "BC:" << aclData->bcFlag << "data len:" << aclData->dataLen;

    if (size < int(sizeof(L2CapHeader))) {
        qCWarning(QT_BT_BLUEZ) << "Unexpected HCI ACL packet size";
        return;
    }
    L2CapHeader l2CapHeader = *reinterpret_cast<const L2CapHeader*>(data);
    l2CapHeader.channelId = qFromLittleEndian(l2CapHeader.channelId);
    l2CapHeader.length = qFromLittleEndian(l2CapHeader.length);
    data += sizeof l2CapHeader;
    size -= sizeof l2CapHeader;
    if (size < l2CapHeader.length) {
        qCWarning(QT_BT_BLUEZ) << "L2Cap payload size" << size << "is smaller than specified size"
                               << l2CapHeader.length;
        return;
    }
//    qCDebug(QT_BT_BLUEZ) << "l2cap channel id:" << l2CapHeader.channelId
//                         << "payload length:" << l2CapHeader.length;
    if (l2CapHeader.channelId != SECURITY_CHANNEL_ID)
        return;
    if (*data != 0xa) // "Signing Information". Spec v4.2, Vol 3, Part H, 3.6.6
        return;
    if (size != 17) {
        qCWarning(QT_BT_BLUEZ) << "Unexpected key size" << size << "in Signing Information packet";
        return;
    }
    quint128 csrk;
    memcpy(&csrk, data + 1, sizeof csrk);
    const bool isRemoteKey = aclData->pbFlag == 2;
    emit signatureResolvingKeyReceived(aclData->handle, isRemoteKey, csrk);
}

void HciManager::handleLeMetaEvent(const quint8 *data)
{
    // Spec v4.2, Vol 2, part E, 7.7.65ff
    switch (*data) {
    case 0x1: {
        const quint16 handle = bt_get_le16(data + 2);
        emit connectionComplete(handle);
        break;
    }
    case 0x3: {
        // TODO: From little endian!
        struct ConnectionUpdateData {
            quint8 status;
            quint16 handle;
            quint16 interval;
            quint16 latency;
            quint16 timeout;
        } __attribute((packed));
        const auto * const updateData
                = reinterpret_cast<const ConnectionUpdateData *>(data + 1);
        if (updateData->status == 0) {
            QLowEnergyConnectionParameters params;
            const double interval = qFromLittleEndian(updateData->interval) * 1.25;
            params.setIntervalRange(interval, interval);
            params.setLatency(qFromLittleEndian(updateData->latency));
            params.setSupervisionTimeout(qFromLittleEndian(updateData->timeout) * 10);
            emit connectionUpdate(qFromLittleEndian(updateData->handle), params);
        }
        break;
    }
    default:
        break;
    }
}

QT_END_NAMESPACE
