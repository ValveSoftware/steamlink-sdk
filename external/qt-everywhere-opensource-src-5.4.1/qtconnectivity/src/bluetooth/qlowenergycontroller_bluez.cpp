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

#include "qlowenergycontroller_p.h"
#include "qbluetoothsocket_p.h"
#include "bluez/bluez_data_p.h"
#include "bluez/hcimanager_p.h"

#include <QtCore/QLoggingCategory>
#include <QtBluetooth/QBluetoothSocket>
#include <QtBluetooth/QLowEnergyService>

#define ATTRIBUTE_CHANNEL_ID 4

#define ATT_DEFAULT_LE_MTU 23
#define ATT_MAX_LE_MTU 0x200

#define GATT_PRIMARY_SERVICE    0x2800
#define GATT_SECONDARY_SERVICE  0x2801
#define GATT_INCLUDED_SERVICE   0x2802
#define GATT_CHARACTERISTIC     0x2803

// GATT commands
#define ATT_OP_ERROR_RESPONSE           0x1
#define ATT_OP_EXCHANGE_MTU_REQUEST     0x2 //send own mtu
#define ATT_OP_EXCHANGE_MTU_RESPONSE    0x3 //receive server MTU
#define ATT_OP_FIND_INFORMATION_REQUEST 0x4 //discover individual attribute info
#define ATT_OP_FIND_INFORMATION_RESPONSE 0x5
#define ATT_OP_READ_BY_TYPE_REQUEST     0x8 //discover characteristics
#define ATT_OP_READ_BY_TYPE_RESPONSE    0x9
#define ATT_OP_READ_REQUEST             0xA //read characteristic & descriptor values
#define ATT_OP_READ_RESPONSE            0xB
#define ATT_OP_READ_BLOB_REQUEST        0xC //read values longer than MTU-1
#define ATT_OP_READ_BLOB_RESPONSE       0xD
#define ATT_OP_READ_BY_GROUP_REQUEST    0x10 //discover services
#define ATT_OP_READ_BY_GROUP_RESPONSE   0x11
#define ATT_OP_WRITE_REQUEST            0x12 //write characteristic with response
#define ATT_OP_WRITE_RESPONSE           0x13
#define ATT_OP_PREPARE_WRITE_REQUEST    0x16 //write values longer than MTU-3 -> queueing
#define ATT_OP_PREPARE_WRITE_RESPONSE   0x17
#define ATT_OP_EXECUTE_WRITE_REQUEST    0x18 //write values longer than MTU-3 -> execute queue
#define ATT_OP_EXECUTE_WRITE_RESPONSE   0x19
#define ATT_OP_HANDLE_VAL_NOTIFICATION  0x1b //informs about value change
#define ATT_OP_HANDLE_VAL_INDICATION    0x1d //informs about value change -> requires reply
#define ATT_OP_HANDLE_VAL_CONFIRMATION  0x1e //answer for ATT_OP_HANDLE_VAL_INDICATION
#define ATT_OP_WRITE_COMMAND            0x52 //write characteristic without response

//GATT command sizes in bytes
#define FIND_INFO_REQUEST_HEADER_SIZE 5
#define GRP_TYPE_REQ_HEADER_SIZE 7
#define READ_BY_TYPE_REQ_HEADER_SIZE 7
#define READ_REQUEST_HEADER_SIZE 3
#define READ_BLOB_REQUEST_HEADER_SIZE 5
#define WRITE_REQUEST_HEADER_SIZE 3    // same size for WRITE_COMMAND header
#define PREPARE_WRITE_HEADER_SIZE 5
#define EXECUTE_WRITE_HEADER_SIZE 2
#define MTU_EXCHANGE_HEADER_SIZE 3

// GATT error codes
#define ATT_ERROR_INVALID_HANDLE        0x01
#define ATT_ERROR_READ_NOT_PERM         0x02
#define ATT_ERROR_WRITE_NOT_PERM        0x03
#define ATT_ERROR_INVALID_PDU           0x04
#define ATT_ERROR_INSUF_AUTHENTICATION  0x05
#define ATT_ERROR_REQUEST_NOT_SUPPORTED 0x06
#define ATT_ERROR_INVALID_OFFSET        0x07
#define ATT_ERROR_INSUF_AUTHORIZATION   0x08
#define ATT_ERROR_PREPARE_QUEUE_FULL    0x09
#define ATT_ERROR_ATTRIBUTE_NOT_FOUND   0x0A
#define ATT_ERROR_ATTRIBUTE_NOT_LONG    0x0B
#define ATT_ERROR_INSUF_ENCR_KEY_SIZE   0x0C
#define ATT_ERROR_INVAL_ATTR_VALUE_LEN  0x0D
#define ATT_ERROR_UNLIKELY              0x0E
#define ATT_ERROR_INSUF_ENCRYPTION      0x0F
#define ATT_ERROR_APPLICATION           0x10
#define ATT_ERROR_INSUF_RESOURCES       0x11

#define APPEND_VALUE true
#define NEW_VALUE false

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

static inline QBluetoothUuid convert_uuid128(const quint128 *p)
{
    quint128 dst_hostOrder, dst_bigEndian;

    // Bluetooth LE data comes as little endian
    // uuids are constructed using high endian
    btoh128(p, &dst_hostOrder);
    hton128(&dst_hostOrder, &dst_bigEndian);

    // convert to Qt's own data type
    quint128 qtdst;
    memcpy(&qtdst, &dst_bigEndian, sizeof(quint128));

    return QBluetoothUuid(qtdst);
}

static void dumpErrorInformation(const QByteArray &response)
{
    const char *data = response.constData();
    if (response.size() != 5 || data[0] != ATT_OP_ERROR_RESPONSE) {
        qCWarning(QT_BT_BLUEZ) << QLatin1String("Not a valid error response");
        return;
    }

    quint8 lastCommand = data[1];
    quint16 handle = bt_get_le16(&data[2]);
    quint8 errorCode = data[4];

    QString errorString;
    switch (errorCode) {
    case ATT_ERROR_INVALID_HANDLE:
        errorString = QStringLiteral("invalid handle"); break;
    case ATT_ERROR_READ_NOT_PERM:
        errorString = QStringLiteral("not readable attribute - permissions"); break;
    case ATT_ERROR_WRITE_NOT_PERM:
        errorString = QStringLiteral("not writable attribute - permissions"); break;
    case ATT_ERROR_INVALID_PDU:
        errorString = QStringLiteral("PDU invalid"); break;
    case ATT_ERROR_INSUF_AUTHENTICATION:
        errorString = QStringLiteral("needs authentication - permissions"); break;
    case ATT_ERROR_REQUEST_NOT_SUPPORTED:
        errorString = QStringLiteral("server does not support request"); break;
    case ATT_ERROR_INVALID_OFFSET:
        errorString = QStringLiteral("offset past end of attribute"); break;
    case ATT_ERROR_INSUF_AUTHORIZATION:
        errorString = QStringLiteral("need authorization - permissions"); break;
    case ATT_ERROR_PREPARE_QUEUE_FULL:
        errorString = QStringLiteral("run out of prepare queue space"); break;
    case ATT_ERROR_ATTRIBUTE_NOT_FOUND:
        errorString = QStringLiteral("no attribute in given range found"); break;
    case ATT_ERROR_ATTRIBUTE_NOT_LONG:
        errorString = QStringLiteral("attribute not read/written using read blob"); break;
    case ATT_ERROR_INSUF_ENCR_KEY_SIZE:
        errorString = QStringLiteral("need encryption key size - permissions"); break;
    case ATT_ERROR_INVAL_ATTR_VALUE_LEN:
        errorString = QStringLiteral("written value is invalid size"); break;
    case ATT_ERROR_UNLIKELY:
        errorString = QStringLiteral("unlikely error"); break;
    case ATT_ERROR_INSUF_ENCRYPTION:
        errorString = QStringLiteral("needs encryption - permissions"); break;
    case ATT_ERROR_APPLICATION:
        errorString = QStringLiteral("application error"); break;
    case ATT_ERROR_INSUF_RESOURCES:
        errorString = QStringLiteral("insufficient resources to complete request"); break;
    default:
        errorString = QStringLiteral("unknown error code"); break;
    }

    qCDebug(QT_BT_BLUEZ) << "Error1:" << errorString
             << "last command:" << hex << lastCommand
             << "handle:" << handle;
}

QLowEnergyControllerPrivate::QLowEnergyControllerPrivate()
    : QObject(),
      state(QLowEnergyController::UnconnectedState),
      error(QLowEnergyController::NoError),
      l2cpSocket(0), requestPending(false),
      mtuSize(ATT_DEFAULT_LE_MTU),
      securityLevelValue(-1),
      encryptionChangePending(false),
      hciManager(0)
{
    qRegisterMetaType<QList<QLowEnergyHandle> >();

    hciManager = new HciManager(localAdapter, this);
    if (!hciManager->isValid())
        return;

    hciManager->monitorEvent(HciManager::EncryptChangeEvent);
    connect(hciManager, SIGNAL(encryptionChangedEvent(QBluetoothAddress,bool)),
            this, SLOT(encryptionChangedEvent(QBluetoothAddress,bool)));
}

QLowEnergyControllerPrivate::~QLowEnergyControllerPrivate()
{
}

void QLowEnergyControllerPrivate::connectToDevice()
{
    setState(QLowEnergyController::ConnectingState);
    if (l2cpSocket)
        delete l2cpSocket;

    l2cpSocket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol, this);
    connect(l2cpSocket, SIGNAL(connected()), this, SLOT(l2cpConnected()));
    connect(l2cpSocket, SIGNAL(disconnected()), this, SLOT(l2cpDisconnected()));
    connect(l2cpSocket, SIGNAL(error(QBluetoothSocket::SocketError)),
            this, SLOT(l2cpErrorChanged(QBluetoothSocket::SocketError)));
    connect(l2cpSocket, SIGNAL(readyRead()), this, SLOT(l2cpReadyRead()));

    if (addressType == QLowEnergyController::PublicAddress)
        l2cpSocket->d_ptr->lowEnergySocketType = BDADDR_LE_PUBLIC;
    else if (addressType == QLowEnergyController::RandomAddress)
        l2cpSocket->d_ptr->lowEnergySocketType = BDADDR_LE_RANDOM;

    int sockfd = l2cpSocket->socketDescriptor();
    if (sockfd < 0) {
        qCWarning(QT_BT_BLUEZ) << "l2cp socket not initialised";
        setError(QLowEnergyController::UnknownError);
        setState(QLowEnergyController::UnconnectedState);
        return;
    }

    struct sockaddr_l2 addr;
    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_cid = htobs(ATTRIBUTE_CHANNEL_ID);
    addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
    convertAddress(localAdapter.toUInt64(), addr.l2_bdaddr.b);

    // bind the socket to the local device
    if (::bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        qCWarning(QT_BT_BLUEZ) << qt_error_string(errno);
        setError(QLowEnergyController::UnknownError);
        setState(QLowEnergyController::UnconnectedState);
        return;
    }

    // connect
    l2cpSocket->connectToService(remoteDevice, ATTRIBUTE_CHANNEL_ID);
}

void QLowEnergyControllerPrivate::l2cpConnected()
{
    Q_Q(QLowEnergyController);

    securityLevelValue = securityLevel();
    exchangeMTU();

    setState(QLowEnergyController::ConnectedState);
    emit q->connected();
}

void QLowEnergyControllerPrivate::disconnectFromDevice()
{
    setState(QLowEnergyController::ClosingState);
    l2cpSocket->close();
    resetController();
}

void QLowEnergyControllerPrivate::l2cpDisconnected()
{
    Q_Q(QLowEnergyController);

    securityLevelValue = -1;
    setState(QLowEnergyController::UnconnectedState);
    emit q->disconnected();
}

void QLowEnergyControllerPrivate::l2cpErrorChanged(QBluetoothSocket::SocketError e)
{
    switch (e) {
    case QBluetoothSocket::HostNotFoundError:
        setError(QLowEnergyController::UnknownRemoteDeviceError);
        qCDebug(QT_BT_BLUEZ) << "The passed remote device address cannot be found";
        break;
    case QBluetoothSocket::NetworkError:
        setError(QLowEnergyController::NetworkError);
        qCDebug(QT_BT_BLUEZ) << "Network IO error while talking to LE device";
        break;
    case QBluetoothSocket::UnknownSocketError:
    case QBluetoothSocket::UnsupportedProtocolError:
    case QBluetoothSocket::OperationError:
    case QBluetoothSocket::ServiceNotFoundError:
    default:
        // these errors shouldn't happen -> as it means
        // the code in this file has bugs
        qCDebug(QT_BT_BLUEZ) << "Unknown l2cp socket error: " << e << l2cpSocket->errorString();
        setError(QLowEnergyController::UnknownError);
        break;
    }

    invalidateServices();
    resetController();
    setState(QLowEnergyController::UnconnectedState);
}


void QLowEnergyControllerPrivate::resetController()
{
    openRequests.clear();
    requestPending = false;
    encryptionChangePending = false;
    securityLevelValue = -1;
}

void QLowEnergyControllerPrivate::l2cpReadyRead()
{
    const QByteArray reply = l2cpSocket->readAll();
    qCDebug(QT_BT_BLUEZ) << "Received size:" << reply.size() << "data:" << reply.toHex();
    if (reply.isEmpty())
        return;

    const quint8 command = reply.constData()[0];
    switch (command) {
    case ATT_OP_HANDLE_VAL_NOTIFICATION:
    {
        processUnsolicitedReply(reply);
        return;
    }
    case ATT_OP_HANDLE_VAL_INDICATION:
    {
        //send confirmation
        QByteArray packet;
        packet.append(static_cast<char>(ATT_OP_HANDLE_VAL_CONFIRMATION));
        sendCommand(packet);

        processUnsolicitedReply(reply);
        return;
    }
    case ATT_OP_EXCHANGE_MTU_REQUEST:
    case ATT_OP_READ_BY_GROUP_REQUEST:
    case ATT_OP_READ_BY_TYPE_REQUEST:
    case ATT_OP_READ_REQUEST:
    case ATT_OP_FIND_INFORMATION_REQUEST:
    case ATT_OP_WRITE_REQUEST:
        qCWarning(QT_BT_BLUEZ) << "Unexpected message type" << hex << command
                               << "will be ignored"   ;
        return;
    default:
        //only solicited replies finish pending requests
        requestPending = false;
        break;
    }

    Q_ASSERT(!openRequests.isEmpty());
    const Request request = openRequests.dequeue();
    processReply(request, reply);

    sendNextPendingRequest();
}

/*!
 * Called when the request for socket encryption has been
 * processed by the kernel. Such requests take time as the kernel
 * has to renegotiate the link parameters with the remote device.
 *
 * Therefore any such request delays the pending ATT commands until this
 * callback is called. The first pending request in the queue is the request
 * that triggered the encryption request.
 */
void QLowEnergyControllerPrivate::encryptionChangedEvent(
        const QBluetoothAddress &address, bool wasSuccess)
{
    if (remoteDevice != address)
        return;

    Q_ASSERT(encryptionChangePending);
    securityLevelValue = securityLevel();

    // On success continue to process ATT command queue
    if (!wasSuccess) {
        // We could not increase the security of the link
        // The next request was requeued due to security error
        // skip it to avoid endless loop of security negotiations
        Q_ASSERT(!openRequests.isEmpty());
        Request failedRequest = openRequests.takeFirst();

        if (failedRequest.command == ATT_OP_WRITE_REQUEST) {
             // Failing write requests trigger some sort of response
            uint ref = failedRequest.reference.toUInt();
            const QLowEnergyHandle charHandle = (ref & 0xffff);
            const QLowEnergyHandle descriptorHandle = ((ref >> 16) & 0xffff);

            QSharedPointer<QLowEnergyServicePrivate> service
                                                = serviceForHandle(charHandle);
            if (!service.isNull() && service->characteristicList.contains(charHandle)) {
                if (!descriptorHandle)
                    service->setError(QLowEnergyService::CharacteristicWriteError);
                else
                    service->setError(QLowEnergyService::DescriptorWriteError);
            }
        } else if (failedRequest.command == ATT_OP_PREPARE_WRITE_REQUEST) {
            uint handleData = failedRequest.reference.toUInt();
            const QLowEnergyHandle attrHandle = (handleData & 0xffff);
            const QByteArray newValue = failedRequest.reference2.toByteArray();

            // Prepare command failed, cancel pending prepare queue on
            // the device. The appropriate (Descriptor|Characteristic)WriteError
            // is emitted too once the execute write request comes through
            sendExecuteWriteRequest(attrHandle, newValue, true);
        }
    }

    encryptionChangePending = false;
    sendNextPendingRequest();
}

void QLowEnergyControllerPrivate::sendCommand(const QByteArray &packet)
{
    qint64 result = l2cpSocket->write(packet.constData(),
                                      packet.size());
    if (result == -1) {
        qCDebug(QT_BT_BLUEZ) << "Cannot write L2CP command:" << hex
                             << packet.toHex()
                             << l2cpSocket->errorString();
        setError(QLowEnergyController::NetworkError);
    }
}

void QLowEnergyControllerPrivate::sendNextPendingRequest()
{
    if (openRequests.isEmpty() || requestPending || encryptionChangePending)
        return;

    const Request &request = openRequests.head();
//    qCDebug(QT_BT_BLUEZ) << "Sending request, type:" << hex << request.command
//             << request.payload.toHex();

    requestPending = true;
    sendCommand(request.payload);
}

QLowEnergyHandle parseReadByTypeCharDiscovery(
        QLowEnergyServicePrivate::CharData *charData,
        const char *data, quint16 elementLength)
{
    Q_ASSERT(charData);
    Q_ASSERT(data);

    QLowEnergyHandle attributeHandle = bt_get_le16(&data[0]);
    charData->properties =
            (QLowEnergyCharacteristic::PropertyTypes)data[2];
    charData->valueHandle = bt_get_le16(&data[3]);

    if (elementLength == 7) // 16 bit uuid
        charData->uuid = QBluetoothUuid(bt_get_le16(&data[5]));
    else
        charData->uuid = convert_uuid128((quint128 *)&data[5]);

    qCDebug(QT_BT_BLUEZ) << "Found handle:" << hex << attributeHandle
             << "properties:" << charData->properties
             << "value handle:" << charData->valueHandle
             << "uuid:" << charData->uuid.toString();

    return attributeHandle;
}

QLowEnergyHandle parseReadByTypeIncludeDiscovery(
        QList<QBluetoothUuid> *foundServices,
        const char *data, quint16 elementLength)
{
    Q_ASSERT(foundServices);
    Q_ASSERT(data);

    QLowEnergyHandle attributeHandle = bt_get_le16(&data[0]);

    // the next 2 elements are not required as we have discovered
    // all (primary/secondary) services already. Now we are only
    // interested in their relationship to each other
    // data[2] -> included service start handle
    // data[4] -> included service end handle

    if (elementLength == 8) //16 bit uuid
        foundServices->append(QBluetoothUuid(bt_get_le16(&data[6])));
    else
        foundServices->append(convert_uuid128((quint128 *) &data[6]));

    qCDebug(QT_BT_BLUEZ) << "Found included service: " << hex
                         << attributeHandle << "uuid:" << *foundServices;

    return attributeHandle;
}

void QLowEnergyControllerPrivate::processReply(
        const Request &request, const QByteArray &response)
{
    Q_Q(QLowEnergyController);

    quint8 command = response.constData()[0];

    bool isErrorResponse = false;
    // if error occurred 2. byte is previous request type
    if (command == ATT_OP_ERROR_RESPONSE) {
        dumpErrorInformation(response);
        command = response.constData()[1];
        isErrorResponse = true;
    }

    switch (command) {
    case ATT_OP_EXCHANGE_MTU_REQUEST: // in case of error
    case ATT_OP_EXCHANGE_MTU_RESPONSE:
    {
        Q_ASSERT(request.command == ATT_OP_EXCHANGE_MTU_REQUEST);
        if (isErrorResponse) {
            mtuSize = ATT_DEFAULT_LE_MTU;
            break;
        }

        const char *data = response.constData();
        quint16 mtu = bt_get_le16(&data[1]);
        mtuSize = mtu;
        if (mtuSize < ATT_DEFAULT_LE_MTU)
            mtuSize = ATT_DEFAULT_LE_MTU;

        qCDebug(QT_BT_BLUEZ) << "Server MTU:" << mtu << "resulting mtu:" << mtuSize;
    }
        break;
    case ATT_OP_READ_BY_GROUP_REQUEST: // in case of error
    case ATT_OP_READ_BY_GROUP_RESPONSE:
    {
        // Discovering services
        Q_ASSERT(request.command == ATT_OP_READ_BY_GROUP_REQUEST);

        const quint16 type = request.reference.toUInt();

        if (isErrorResponse) {
            if (type == GATT_SECONDARY_SERVICE) {
                setState(QLowEnergyController::DiscoveredState);
                q->discoveryFinished();
            } else { // search for secondary services
                sendReadByGroupRequest(0x0001, 0xFFFF, GATT_SECONDARY_SERVICE);
            }
            break;
        }

        QLowEnergyHandle start = 0, end = 0;
        const quint16 elementLength = response.constData()[1];
        const quint16 numElements = (response.size() - 2) / elementLength;
        quint16 offset = 2;
        const char *data = response.constData();
        for (int i = 0; i < numElements; i++) {
            start = bt_get_le16(&data[offset]);
            end = bt_get_le16(&data[offset+2]);

            QBluetoothUuid uuid;
            if (elementLength == 6) //16 bit uuid
                uuid = QBluetoothUuid(bt_get_le16(&data[offset+4]));
            else if (elementLength == 20) //128 bit uuid
                uuid = convert_uuid128((quint128 *)&data[offset+4]);
            //else -> do nothing

            offset += elementLength;


            qCDebug(QT_BT_BLUEZ) << "Found uuid:" << uuid << "start handle:" << hex
                     << start << "end handle:" << end;

            QLowEnergyServicePrivate *priv = new QLowEnergyServicePrivate();
            priv->uuid = uuid;
            priv->startHandle = start;
            priv->endHandle = end;
            if (type != GATT_PRIMARY_SERVICE) //unset PrimaryService bit
                priv->type &= ~QLowEnergyService::PrimaryService;
            priv->setController(this);

            QSharedPointer<QLowEnergyServicePrivate> pointer(priv);

            serviceList.insert(uuid, pointer);
            emit q->serviceDiscovered(uuid);
        }

        if (end != 0xFFFF) {
            sendReadByGroupRequest(end+1, 0xFFFF, type);
        } else {
            if (type == GATT_SECONDARY_SERVICE) {
                setState(QLowEnergyController::DiscoveredState);
                emit q->discoveryFinished();
            } else { // search for secondary services
                sendReadByGroupRequest(0x0001, 0xFFFF, GATT_SECONDARY_SERVICE);
            }
        }
    }
        break;
    case ATT_OP_READ_BY_TYPE_REQUEST: //in case of error
    case ATT_OP_READ_BY_TYPE_RESPONSE:
    {
        // Discovering characteristics
        Q_ASSERT(request.command == ATT_OP_READ_BY_TYPE_REQUEST);

        QSharedPointer<QLowEnergyServicePrivate> p =
                request.reference.value<QSharedPointer<QLowEnergyServicePrivate> >();
        const quint16 attributeType = request.reference2.toUInt();

        if (isErrorResponse) {
            if (attributeType == GATT_CHARACTERISTIC) {
                // we reached end of service handle
                // just finished up characteristic discovery
                // continue with values of characteristics
                if (!p->characteristicList.isEmpty()) {
                    readServiceValues(p->uuid, true);
                } else {
                    // discovery finished since the service doesn't have any
                    // characteristics
                    p->setState(QLowEnergyService::ServiceDiscovered);
                }
            } else if (attributeType == GATT_INCLUDED_SERVICE) {
                // finished up include discovery
                // continue with characteristic discovery
                sendReadByTypeRequest(p, p->startHandle, GATT_CHARACTERISTIC);
            }
            break;
        }

        /* packet format:
         * if GATT_CHARACTERISTIC discovery
         *      <opcode><elementLength>
         *          [<handle><property><charHandle><uuid>]+
         *
         * if GATT_INCLUDE discovery
         *      <opcode><elementLength>
         *          [<handle><startHandle_included><endHandle_included><uuid>]+
         *
         *  The uuid can be 16 or 128 bit.
         */
        QLowEnergyHandle lastHandle;
        const quint16 elementLength = response.constData()[1];
        const quint16 numElements = (response.size() - 2) / elementLength;
        quint16 offset = 2;
        const char *data = response.constData();
        for (int i = 0; i < numElements; i++) {
            if (attributeType == GATT_CHARACTERISTIC) {
                QLowEnergyServicePrivate::CharData characteristic;
                lastHandle = parseReadByTypeCharDiscovery(
                            &characteristic, &data[offset], elementLength);
                p->characteristicList[lastHandle] = characteristic;
            } else if (attributeType == GATT_INCLUDED_SERVICE) {
                QList<QBluetoothUuid> includedServices;
                lastHandle = parseReadByTypeIncludeDiscovery(
                            &includedServices, &data[offset], elementLength);
                p->includedServices = includedServices;
                foreach (const QBluetoothUuid &uuid, includedServices) {
                    if (serviceList.contains(uuid))
                        serviceList[uuid]->type |= QLowEnergyService::IncludedService;
                }
            }
        }

        if (lastHandle + 1 < p->endHandle) { // more chars to discover
            sendReadByTypeRequest(p, lastHandle + 1, attributeType);
        } else {
            if (attributeType == GATT_INCLUDED_SERVICE)
                sendReadByTypeRequest(p, p->startHandle, GATT_CHARACTERISTIC);
            else
                readServiceValues(p->uuid, true);
        }
    }
        break;
    case ATT_OP_READ_REQUEST: //error case
    case ATT_OP_READ_RESPONSE:
    {
        //Reading characteristics and descriptors
        Q_ASSERT(request.command == ATT_OP_READ_REQUEST);

        uint handleData = request.reference.toUInt();
        const QLowEnergyHandle charHandle = (handleData & 0xffff);
        const QLowEnergyHandle descriptorHandle = ((handleData >> 16) & 0xffff);

        if (isErrorResponse) {
            Q_ASSERT(!encryptionChangePending);
            encryptionChangePending = increaseEncryptLevelfRequired(response.constData()[4]);
            if (encryptionChangePending) {
                // Just requested a security level change.
                // Retry the same command again once the change has happened
                openRequests.prepend(request);
                break;
            }
        } else {
            if (!descriptorHandle)
                updateValueOfCharacteristic(charHandle, response.mid(1), NEW_VALUE);
            else
                updateValueOfDescriptor(charHandle, descriptorHandle,
                                        response.mid(1), NEW_VALUE);

            if (response.size() == mtuSize) {
                // Potentially more data -> switch to blob reads
                readServiceValuesByOffset(handleData, mtuSize-1,
                                          request.reference2.toBool());
                break;
            }
        }

        if (request.reference2.toBool()) {
            //last characteristic -> progress to descriptor discovery
            //last descriptor -> service discovery is done
            QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
            Q_ASSERT(!service.isNull());
            if (!descriptorHandle)
                discoverServiceDescriptors(service->uuid);
            else
                service->setState(QLowEnergyService::ServiceDiscovered);
        }
    }
        break;
    case ATT_OP_READ_BLOB_REQUEST: //error case
    case ATT_OP_READ_BLOB_RESPONSE:
    {
        //Reading characteristic or descriptor with value longer than MTU
        Q_ASSERT(request.command == ATT_OP_READ_BLOB_REQUEST);

        uint handleData = request.reference.toUInt();
        const QLowEnergyHandle charHandle = (handleData & 0xffff);
        const QLowEnergyHandle descriptorHandle = ((handleData >> 16) & 0xffff);

        /*
         * READ_BLOB does not require encryption setup code. BLOB commands
         * are only issued after read request if the read request is too long
         * for single MTU. The preceding read request would have triggered
         * the setup of the encryption already.
         */
        if (!isErrorResponse) {
            quint16 length = 0;
            if (!descriptorHandle)
                length = updateValueOfCharacteristic(charHandle, response.mid(1), APPEND_VALUE);
            else
                length = updateValueOfDescriptor(charHandle, descriptorHandle,
                                        response.mid(1), APPEND_VALUE);
            if (response.size() == mtuSize) {
                readServiceValuesByOffset(handleData, length,
                                          request.reference2.toBool());
                break;
            }
        }

        if (request.reference2.toBool()) {
            //last overlong characteristic -> progress to descriptor discovery
            //last overlong descriptor -> service discovery is done
            QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
            Q_ASSERT(!service.isNull());
            if (!descriptorHandle)
                discoverServiceDescriptors(service->uuid);
            else
                service->setState(QLowEnergyService::ServiceDiscovered);
        }

    }
        break;
    case ATT_OP_FIND_INFORMATION_REQUEST: //error case
    case ATT_OP_FIND_INFORMATION_RESPONSE:
    {
        //Discovering descriptors
        Q_ASSERT(request.command == ATT_OP_FIND_INFORMATION_REQUEST);

        /* packet format:
         *  <opcode><format>[<handle><descriptor_uuid>]+
         *
         *  The uuid can be 16 or 128 bit which is indicated by format.
         */

        QList<QLowEnergyHandle> keys = request.reference.value<QList<QLowEnergyHandle> >();
        if (keys.isEmpty()) {
            qCWarning(QT_BT_BLUEZ) << "Descriptor discovery for unknown characteristic received";
            break;
        }
        QLowEnergyHandle charHandle = keys.first();

        QSharedPointer<QLowEnergyServicePrivate> p =
                serviceForHandle(charHandle);
        Q_ASSERT(!p.isNull());

        if (isErrorResponse) {
            readServiceValues(p->uuid, false); //read descriptor values
            break;
        }

        const quint8 format = response[1];
        quint16 elementLength;
        switch (format) {
        case 0x01:
            elementLength = 2 + 2; //sizeof(QLowEnergyHandle) + 16bit uuid
            break;
        case 0x02:
            elementLength = 2 + 16; //sizeof(QLowEnergyHandle) + 128bit uuid
            break;
        default:
            qCWarning(QT_BT_BLUEZ) << "Unknown format in FIND_INFORMATION_RESPONSE";
            return;
        }

        const quint16 numElements = (response.size() - 2) / elementLength;

        quint16 offset = 2;
        QLowEnergyHandle descriptorHandle;
        QBluetoothUuid uuid;
        const char *data = response.constData();
        for (int i = 0; i < numElements; i++) {
            descriptorHandle = bt_get_le16(&data[offset]);

            if (format == 0x01)
                uuid = QBluetoothUuid(bt_get_le16(&data[offset+2]));
            else if (format == 0x02)
                uuid = convert_uuid128((quint128 *)&data[offset+2]);

            offset += elementLength;

            // ignore all attributes which are not of type descriptor
            // examples are the characteristics value or
            bool ok = false;
            quint16 shortUuid = uuid.toUInt16(&ok);
            if (ok && shortUuid >= QLowEnergyServicePrivate::PrimaryService
                   && shortUuid <= QLowEnergyServicePrivate::Characteristic){
                qCDebug(QT_BT_BLUEZ) << "Suppressing primary/characteristic" << hex << shortUuid;
                continue;
            }

            // ignore value handle
            if (descriptorHandle == p->characteristicList[charHandle].valueHandle) {
                qCDebug(QT_BT_BLUEZ) << "Suppressing char handle" << hex << descriptorHandle;
                continue;
            }

            QLowEnergyServicePrivate::DescData data;
            data.uuid = uuid;
            p->characteristicList[charHandle].descriptorList.insert(
                        descriptorHandle, data);

            qCDebug(QT_BT_BLUEZ) << "Descriptor found, uuid:"
                                 << uuid.toString()
                                 << "descriptor handle:" << hex << descriptorHandle;
        }

        const QLowEnergyHandle nextPotentialHandle = descriptorHandle + 1;
        if (keys.count() == 1) {
            // Reached last characteristic of service

            // The endhandle of a service is always the last handle of
            // the current service. We must either continue until we have reached
            // the starting handle of the next service (endHandle+1) or
            // the last physical handle address (0xffff). Note that
            // the endHandle of the last service on the device is 0xffff.

            if ((p->endHandle != 0xffff && nextPotentialHandle >= p->endHandle + 1)
                    || (descriptorHandle == 0xffff)) {
                keys.removeFirst();
                // last descriptor of last characteristic found
                // continue with reading descriptor values
                readServiceValues(p->uuid, false);
            } else {
                discoverNextDescriptor(p, keys, nextPotentialHandle);
            }
        } else {
            if (nextPotentialHandle >= keys[1]) //reached next char
                keys.removeFirst();
            discoverNextDescriptor(p, keys, nextPotentialHandle);
        }
    }
        break;
    case ATT_OP_WRITE_REQUEST: //error case
    case ATT_OP_WRITE_RESPONSE:
    {
        //Write command response
        Q_ASSERT(request.command == ATT_OP_WRITE_REQUEST);

        uint ref = request.reference.toUInt();
        const QLowEnergyHandle charHandle = (ref & 0xffff);
        const QLowEnergyHandle descriptorHandle = ((ref >> 16) & 0xffff);

        QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
        if (service.isNull() || !service->characteristicList.contains(charHandle))
            break;

        if (isErrorResponse) {
            Q_ASSERT(!encryptionChangePending);
            encryptionChangePending = increaseEncryptLevelfRequired(response.constData()[4]);
            if (encryptionChangePending) {
                openRequests.prepend(request);
                break;
            }

            if (!descriptorHandle)
                service->setError(QLowEnergyService::CharacteristicWriteError);
            else
                service->setError(QLowEnergyService::DescriptorWriteError);
            break;
        }

        const QByteArray newValue = request.reference2.toByteArray();
        if (!descriptorHandle) {
            updateValueOfCharacteristic(charHandle, newValue, NEW_VALUE);
            QLowEnergyCharacteristic ch(service, charHandle);
            emit service->characteristicWritten(ch, newValue);
        } else {
            updateValueOfDescriptor(charHandle, descriptorHandle, newValue, NEW_VALUE);
            QLowEnergyDescriptor descriptor(service, charHandle, descriptorHandle);
            emit service->descriptorWritten(descriptor, newValue);
        }
    }
        break;
    case ATT_OP_PREPARE_WRITE_REQUEST: //error case
    case ATT_OP_PREPARE_WRITE_RESPONSE:
    {
        //Prepare write command response
        Q_ASSERT(request.command == ATT_OP_PREPARE_WRITE_REQUEST);

        uint handleData = request.reference.toUInt();
        const QLowEnergyHandle attrHandle = (handleData & 0xffff);
        const QByteArray newValue = request.reference2.toByteArray();
        const int writtenPayload = ((handleData >> 16) & 0xffff);

        if (isErrorResponse) {
            Q_ASSERT(!encryptionChangePending);
            encryptionChangePending = increaseEncryptLevelfRequired(response.constData()[4]);
            if (encryptionChangePending) {
                openRequests.prepend(request);
                break;
            }
            //emits error on cancellation and aborts existing prepare reuqests
            sendExecuteWriteRequest(attrHandle, newValue, true);
        } else {
            if (writtenPayload < newValue.size()) {
                sendNextPrepareWriteRequest(attrHandle, newValue, writtenPayload);
            } else {
                sendExecuteWriteRequest(attrHandle, newValue, false);
            }
        }
    }
        break;
    case ATT_OP_EXECUTE_WRITE_REQUEST: //error case
    case ATT_OP_EXECUTE_WRITE_RESPONSE:
    {
        // right now used in connection with long characteristic/descriptor value writes
        // not catering for reliable writes
        Q_ASSERT(request.command == ATT_OP_EXECUTE_WRITE_REQUEST);

        uint handleData = request.reference.toUInt();
        const QLowEnergyHandle attrHandle = handleData & 0xffff;
        bool wasCancellation = !((handleData >> 16) & 0xffff);
        const QByteArray newValue = request.reference2.toByteArray();

        // is it a descriptor or characteristic?
        const QLowEnergyDescriptor descriptor = descriptorForHandle(attrHandle);
        QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(attrHandle);
        Q_ASSERT(!service.isNull());

        if (isErrorResponse || wasCancellation) {
            // charHandle == 0 -> cancellation
            if (descriptor.isValid())
                service->setError(QLowEnergyService::DescriptorWriteError);
            else
                service->setError(QLowEnergyService::CharacteristicWriteError);
        } else {
            if (descriptor.isValid()) {
                updateValueOfDescriptor(descriptor.characteristicHandle(),
                                        attrHandle, newValue, NEW_VALUE);
                emit service->descriptorWritten(descriptor, newValue);
            } else {
                updateValueOfCharacteristic(attrHandle, newValue, NEW_VALUE);
                QLowEnergyCharacteristic ch(service, attrHandle);
                emit service->characteristicWritten(ch, newValue);
            }
        }
    }
        break;
    default:
        qCDebug(QT_BT_BLUEZ) << "Unknown packet: " << response.toHex();
        break;
    }
}

void QLowEnergyControllerPrivate::discoverServices()
{
    sendReadByGroupRequest(0x0001, 0xFFFF, GATT_PRIMARY_SERVICE);
}

void QLowEnergyControllerPrivate::sendReadByGroupRequest(
        QLowEnergyHandle start, QLowEnergyHandle end, quint16 type)
{
    //call for primary and secondary services
    quint8 packet[GRP_TYPE_REQ_HEADER_SIZE];

    packet[0] = ATT_OP_READ_BY_GROUP_REQUEST;
    bt_put_unaligned(htobs(start), (quint16 *) &packet[1]);
    bt_put_unaligned(htobs(end), (quint16 *) &packet[3]);
    bt_put_unaligned(htobs(type), (quint16 *) &packet[5]);

    QByteArray data(GRP_TYPE_REQ_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet,  GRP_TYPE_REQ_HEADER_SIZE);
    qCDebug(QT_BT_BLUEZ) << "Sending read_by_group_type request, startHandle:" << hex
             << start << "endHandle:" << end << type;

    Request request;
    request.payload = data;
    request.command = ATT_OP_READ_BY_GROUP_REQUEST;
    request.reference = type;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

void QLowEnergyControllerPrivate::discoverServiceDetails(const QBluetoothUuid &service)
{
    if (!serviceList.contains(service)) {
        qCWarning(QT_BT_BLUEZ) << "Discovery of unknown service" << service.toString()
                               << "not possible";
        return;
    }

    QSharedPointer<QLowEnergyServicePrivate> serviceData = serviceList.value(service);
    serviceData->characteristicList.clear();
    sendReadByTypeRequest(serviceData, serviceData->startHandle, GATT_INCLUDED_SERVICE);
}

void QLowEnergyControllerPrivate::sendReadByTypeRequest(
        QSharedPointer<QLowEnergyServicePrivate> serviceData,
        QLowEnergyHandle nextHandle, quint16 attributeType)
{
    quint8 packet[READ_BY_TYPE_REQ_HEADER_SIZE];

    packet[0] = ATT_OP_READ_BY_TYPE_REQUEST;
    bt_put_unaligned(htobs(nextHandle), (quint16 *) &packet[1]);
    bt_put_unaligned(htobs(serviceData->endHandle), (quint16 *) &packet[3]);
    bt_put_unaligned(htobs(attributeType), (quint16 *) &packet[5]);

    QByteArray data(READ_BY_TYPE_REQ_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet,  READ_BY_TYPE_REQ_HEADER_SIZE);
    qCDebug(QT_BT_BLUEZ) << "Sending read_by_type request, startHandle:" << hex
             << nextHandle << "endHandle:" << serviceData->endHandle
             << "type:" << attributeType << "packet:" << data.toHex();

    Request request;
    request.payload = data;
    request.command = ATT_OP_READ_BY_TYPE_REQUEST;
    request.reference = QVariant::fromValue(serviceData);
    request.reference2 = attributeType;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

/*!
    \internal

    Reads the value of characteristics and descriptors.

    \a readCharacteristics determines whether we intend to read a characteristic;
    otherwise we read a descriptor.
 */
void QLowEnergyControllerPrivate::readServiceValues(
        const QBluetoothUuid &serviceUuid, bool readCharacteristics)
{
    quint8 packet[READ_REQUEST_HEADER_SIZE];
    if (QT_BT_BLUEZ().isDebugEnabled()) {
        if (readCharacteristics)
            qCDebug(QT_BT_BLUEZ) << "Reading characteristic values for"
                         << serviceUuid.toString();
        else
            qCDebug(QT_BT_BLUEZ) << "Reading descriptor values for"
                         << serviceUuid.toString();
    }

    QSharedPointer<QLowEnergyServicePrivate> service = serviceList.value(serviceUuid);

    // pair.first -> target attribute
    // pair.second -> context information for read request
    QPair<QLowEnergyHandle, quint32> pair;

    // Create list of attribute handles which need to be read
    QList<QPair<QLowEnergyHandle, quint32> > targetHandles;
    const QList<QLowEnergyHandle> keys = service->characteristicList.keys();
    for (int i = 0; i < keys.count(); i++) {
        const QLowEnergyHandle charHandle = keys[i];
        const QLowEnergyServicePrivate::CharData &charDetails =
                service->characteristicList[charHandle];


        if (readCharacteristics) {
            // Collect handles of all characteristic value attributes

            // Don't try to read writeOnly characteristic
            if (!(charDetails.properties & QLowEnergyCharacteristic::Read))
                continue;

            pair.first = charDetails.valueHandle;
            pair.second  = charHandle;
            targetHandles.append(pair);

        } else {
            // Collect handles of all descriptor attributes
            foreach (QLowEnergyHandle descriptorHandle, charDetails.descriptorList.keys()) {
                pair.first = descriptorHandle;
                pair.second = (charHandle | (descriptorHandle << 16));
                targetHandles.append(pair);
            }
        }
    }


    if (targetHandles.isEmpty()) {
        if (readCharacteristics) {
            // none of the characteristics is readable
            // -> continue with descriptor discovery
            discoverServiceDescriptors(service->uuid);
        } else {
            // characteristic w/o descriptors
            service->setState(QLowEnergyService::ServiceDiscovered);
        }
        return;
    }

    for (int i = 0; i < targetHandles.count(); i++) {
        pair = targetHandles.at(i);
        packet[0] = ATT_OP_READ_REQUEST;
        bt_put_unaligned(htobs(pair.first), (quint16 *) &packet[1]);

        QByteArray data(READ_REQUEST_HEADER_SIZE, Qt::Uninitialized);
        memcpy(data.data(), packet,  READ_REQUEST_HEADER_SIZE);

        Request request;
        request.payload = data;
        request.command = ATT_OP_READ_REQUEST;
        request.reference = pair.second;
        // last entry?
        request.reference2 = QVariant((bool)(i + 1 == targetHandles.count()));
        openRequests.enqueue(request);
    }

    sendNextPendingRequest();
}

/*!
    \internal

    This function is used when reading a handle value that is
    longer than the mtuSize.

    The BLOB read request is prepended to the list of
    open requests to finish the current value read up before
    starting the next read request.
 */
void QLowEnergyControllerPrivate::readServiceValuesByOffset(
        quint16 handleData, quint16 offset, bool isLastValue)
{
    const QLowEnergyHandle charHandle = (handleData & 0xffff);
    const QLowEnergyHandle descriptorHandle = ((handleData >> 16) & 0xffff);
    quint8 packet[READ_REQUEST_HEADER_SIZE];

    packet[0] = ATT_OP_READ_BLOB_REQUEST;

    QLowEnergyHandle handleToRead = charHandle;
    if (descriptorHandle) {
        handleToRead = descriptorHandle;
        qCDebug(QT_BT_BLUEZ) << "Reading descriptor via blob request"
                             << hex << descriptorHandle;
    } else {
        //charHandle is not the char's value handle
        QSharedPointer<QLowEnergyServicePrivate> service =
                serviceForHandle(charHandle);
        if (!service.isNull()
                && service->characteristicList.contains(charHandle)) {
            handleToRead = service->characteristicList[charHandle].valueHandle;
            qCDebug(QT_BT_BLUEZ) << "Reading characteristic via blob request"
                                 << hex << handleToRead;
        } else {
            Q_ASSERT(false);
        }
    }

    bt_put_unaligned(htobs(handleToRead), (quint16 *) &packet[1]);
    bt_put_unaligned(htobs(offset), (quint16 *) &packet[3]);

    QByteArray data(READ_BLOB_REQUEST_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet, READ_BLOB_REQUEST_HEADER_SIZE);

    Request request;
    request.payload = data;
    request.command = ATT_OP_READ_BLOB_REQUEST;
    request.reference = handleData;
    request.reference2 = isLastValue;
    openRequests.prepend(request);
}

void QLowEnergyControllerPrivate::discoverServiceDescriptors(
        const QBluetoothUuid &serviceUuid)
{
    qCDebug(QT_BT_BLUEZ) << "Discovering descriptor values for"
                         << serviceUuid.toString();
    QSharedPointer<QLowEnergyServicePrivate> service = serviceList.value(serviceUuid);
    // start handle of all known characteristics
    QList<QLowEnergyHandle> keys = service->characteristicList.keys();

    if (keys.isEmpty()) { // service has no characteristics
        // implies that characteristic & descriptor discovery can be skipped
        service->setState(QLowEnergyService::ServiceDiscovered);
        return;
    }

    std::sort(keys.begin(), keys.end());

    discoverNextDescriptor(service, keys, keys[0]);
}

void QLowEnergyControllerPrivate::processUnsolicitedReply(const QByteArray &payload)
{
    const char *data = payload.constData();
    bool isNotification = (data[0] == ATT_OP_HANDLE_VAL_NOTIFICATION);
    const QLowEnergyHandle changedHandle = bt_get_le16(&data[1]);

    if (QT_BT_BLUEZ().isDebugEnabled()) {
        if (isNotification)
            qCDebug(QT_BT_BLUEZ) << "Change notification for handle" << hex << changedHandle;
        else
            qCDebug(QT_BT_BLUEZ) << "Change indication for handle" << hex << changedHandle;
    }

    const QLowEnergyCharacteristic ch = characteristicForHandle(changedHandle);
    if (ch.isValid() && ch.handle() == changedHandle) {
        updateValueOfCharacteristic(ch.attributeHandle(), payload.mid(3), NEW_VALUE);
        emit ch.d_ptr->characteristicChanged(ch, payload.mid(3));
    } else {
        qCWarning(QT_BT_BLUEZ) << "Cannot find matching characteristic for "
                                  "notification/indication";
    }
}

void QLowEnergyControllerPrivate::exchangeMTU()
{
    qCDebug(QT_BT_BLUEZ) << "Exchanging MTU";

    quint8 packet[MTU_EXCHANGE_HEADER_SIZE];
    packet[0] = ATT_OP_EXCHANGE_MTU_REQUEST;
    bt_put_unaligned(htobs(ATT_MAX_LE_MTU), (quint16 *) &packet[1]);

    QByteArray data(MTU_EXCHANGE_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet, MTU_EXCHANGE_HEADER_SIZE);

    Request request;
    request.payload = data;
    request.command = ATT_OP_EXCHANGE_MTU_REQUEST;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

int QLowEnergyControllerPrivate::securityLevel() const
{
    int socket = l2cpSocket->socketDescriptor();
    if (socket < 0) {
        qCWarning(QT_BT_BLUEZ) << "Invalid l2cp socket, aborting getting of sec level";
        return -1;
    }

    struct bt_security secData;
    socklen_t length = sizeof(secData);
    memset(&secData, 0, length);

    if (getsockopt(socket, SOL_BLUETOOTH, BT_SECURITY, &secData, &length) == 0) {
        qCDebug(QT_BT_BLUEZ) << "Current l2cp sec level:" << secData.level;
        return secData.level;
    }

    if (errno != ENOPROTOOPT) //older kernel, fall back to L2CAP_LM option
        return -1;

    // cater for older kernels
    int optval;
    length = sizeof(optval);
    if (getsockopt(socket, SOL_L2CAP, L2CAP_LM, &optval, &length) == 0) {
        int level = BT_SECURITY_SDP;
        if (optval & L2CAP_LM_AUTH)
            level = BT_SECURITY_LOW;
        if (optval & L2CAP_LM_ENCRYPT)
            level = BT_SECURITY_MEDIUM;
        if (optval & L2CAP_LM_SECURE)
            level = BT_SECURITY_HIGH;

        qDebug() << "Current l2cp sec level (old):" << level;
        return level;
    }

    return -1;
}

bool QLowEnergyControllerPrivate::setSecurityLevel(int level)
{
    if (level > BT_SECURITY_HIGH || level < BT_SECURITY_LOW)
        return false;

    int socket = l2cpSocket->socketDescriptor();
    if (socket < 0) {
        qCWarning(QT_BT_BLUEZ) << "Invalid l2cp socket, aborting setting of sec level";
        return false;
    }

    struct bt_security secData;
    socklen_t length = sizeof(secData);
    memset(&secData, 0, length);
    secData.level = level;

    if (setsockopt(socket, SOL_BLUETOOTH, BT_SECURITY, &secData, length) == 0) {
        qCDebug(QT_BT_BLUEZ) << "Setting new l2cp sec level:" << secData.level;
        return true;
    }

    if (errno != ENOPROTOOPT) //older kernel
        return false;

    int optval = 0;
    switch (level) { // fall through intendeds
        case BT_SECURITY_HIGH:
            optval |= L2CAP_LM_SECURE;
        case BT_SECURITY_MEDIUM:
            optval |= L2CAP_LM_ENCRYPT;
        case BT_SECURITY_LOW:
            optval |= L2CAP_LM_AUTH;
            break;
        default:
            return false;
    }

    if (setsockopt(socket, SOL_L2CAP, L2CAP_LM, &optval, sizeof(optval)) == 0) {
        qDebug(QT_BT_BLUEZ) << "Old l2cp sec level:" << optval;
        return true;
    }

    return false;
}

void QLowEnergyControllerPrivate::discoverNextDescriptor(
        QSharedPointer<QLowEnergyServicePrivate> serviceData,
        const QList<QLowEnergyHandle> pendingCharHandles,
        const QLowEnergyHandle startingHandle)
{
    Q_ASSERT(!pendingCharHandles.isEmpty());
    Q_ASSERT(!serviceData.isNull());

    qCDebug(QT_BT_BLUEZ) << "Sending find_info request" << hex
                         << pendingCharHandles << startingHandle;

    quint8 packet[FIND_INFO_REQUEST_HEADER_SIZE];
    packet[0] = ATT_OP_FIND_INFORMATION_REQUEST;

    const QLowEnergyHandle charStartHandle = startingHandle;
    QLowEnergyHandle charEndHandle = 0;
    if (pendingCharHandles.count() == 1) //single characteristic
        charEndHandle = serviceData->endHandle;
    else
        charEndHandle = pendingCharHandles[1] - 1;

    bt_put_unaligned(htobs(charStartHandle), (quint16 *) &packet[1]);
    bt_put_unaligned(htobs(charEndHandle), (quint16 *) &packet[3]);

    QByteArray data(FIND_INFO_REQUEST_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet, FIND_INFO_REQUEST_HEADER_SIZE);

    Request request;
    request.payload = data;
    request.command = ATT_OP_FIND_INFORMATION_REQUEST;
    request.reference = QVariant::fromValue<QList<QLowEnergyHandle> >(pendingCharHandles);
    request.reference2 = startingHandle;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

void QLowEnergyControllerPrivate::sendNextPrepareWriteRequest(
        const QLowEnergyHandle handle, const QByteArray &newValue,
        quint16 offset)
{
    // is it a descriptor or characteristic?
    QLowEnergyHandle targetHandle = 0;
    const QLowEnergyDescriptor descriptor = descriptorForHandle(handle);
    if (descriptor.isValid())
        targetHandle = descriptor.handle();
    else
        targetHandle = characteristicForHandle(handle).handle();

    if (!targetHandle) {
        qCWarning(QT_BT_BLUEZ) << "sendNextPrepareWriteRequest cancelled due to invalid handle"
                               << handle;
        return;
    }

    quint8 packet[PREPARE_WRITE_HEADER_SIZE];
    packet[0] = ATT_OP_PREPARE_WRITE_REQUEST;
    bt_put_unaligned(htobs(targetHandle), (quint16 *) &packet[1]); // attribute handle
    bt_put_unaligned(htobs(offset), (quint16 *) &packet[3]); // offset into newValue

    qCDebug(QT_BT_BLUEZ) << "Writing long characteristic (prepare):"
                         << hex << handle;


    const int maxAvailablePayload = mtuSize - PREPARE_WRITE_HEADER_SIZE;
    const int requiredPayload = qMin(newValue.size() - offset, maxAvailablePayload);
    const int dataSize = PREPARE_WRITE_HEADER_SIZE + requiredPayload;

    Q_ASSERT((offset + requiredPayload) <= newValue.size());
    Q_ASSERT(dataSize <= mtuSize);

    QByteArray data(dataSize, Qt::Uninitialized);
    memcpy(data.data(), packet, PREPARE_WRITE_HEADER_SIZE);
    memcpy(&(data.data()[PREPARE_WRITE_HEADER_SIZE]), &(newValue.constData()[offset]),
                                               requiredPayload);

    Request request;
    request.payload = data;
    request.command = ATT_OP_PREPARE_WRITE_REQUEST;
    request.reference = (handle | ((offset + requiredPayload) << 16));
    request.reference2 = newValue;
    openRequests.enqueue(request);
}

/*!
    Sends an "Execute Write Request" for a long characteristic or descriptor write.
    This cannot be used for executes in relation to reliable write requests.

    A cancellation removes all pending prepare write request on the GATT server.
    Otherwise this function sends an execute request for all pending prepare
    write requests.
 */
void QLowEnergyControllerPrivate::sendExecuteWriteRequest(
        const QLowEnergyHandle attrHandle, const QByteArray &newValue,
        bool isCancelation)
{
    quint8 packet[EXECUTE_WRITE_HEADER_SIZE];
    packet[0] = ATT_OP_EXECUTE_WRITE_REQUEST;
    if (isCancelation)
        packet[1] = 0x00; // cancel pending write prepare requests
    else
        packet[1] = 0x01; // execute pending write prepare requests

    QByteArray data(EXECUTE_WRITE_HEADER_SIZE, Qt::Uninitialized);
    memcpy(data.data(), packet, EXECUTE_WRITE_HEADER_SIZE);

    qCDebug(QT_BT_BLUEZ) << "Sending Execute Write Request for long characteristic value"
                         << hex << attrHandle;

    Request request;
    request.payload = data;
    request.command = ATT_OP_EXECUTE_WRITE_REQUEST;
    request.reference  = (attrHandle | ((isCancelation ? 0x00 : 0x01) << 16));
    request.reference2 = newValue;
    openRequests.prepend(request);
}


/*!
    Writes long (prepare write request), short (write request)
    and writeWithoutResponse characteristic values.

    TODO Reliable/prepare write across multiple characteristics is not supported
 */
void QLowEnergyControllerPrivate::writeCharacteristic(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle charHandle,
        const QByteArray &newValue,
        bool writeWithResponse)
{
    Q_ASSERT(!service.isNull());

    if (!service->characteristicList.contains(charHandle))
        return;

    const QLowEnergyHandle valueHandle = service->characteristicList[charHandle].valueHandle;
    const int size = WRITE_REQUEST_HEADER_SIZE + newValue.size();

    quint8 packet[WRITE_REQUEST_HEADER_SIZE];
    if (writeWithResponse) {
        if (newValue.size() > (mtuSize - WRITE_REQUEST_HEADER_SIZE)) {
            sendNextPrepareWriteRequest(charHandle, newValue, 0);
            sendNextPendingRequest();
            return;
        } else {
            // write value fits into single package
            packet[0] = ATT_OP_WRITE_REQUEST;
        }
    } else {
        // write without response
        packet[0] = ATT_OP_WRITE_COMMAND;
    }

    bt_put_unaligned(htobs(valueHandle), (quint16 *) &packet[1]);

    QByteArray data(size, Qt::Uninitialized);
    memcpy(data.data(), packet, WRITE_REQUEST_HEADER_SIZE);
    memcpy(&(data.data()[WRITE_REQUEST_HEADER_SIZE]), newValue.constData(), newValue.size());

    qCDebug(QT_BT_BLUEZ) << "Writing characteristic" << hex << charHandle
                         << "(size:" << size << "with response:" << writeWithResponse << ")";

    // Advantage of write without response is the quick turnaround.
    // It can be send at any time and does not produce responses.
    // Therefore we will not put them into the openRequest queue at all.
    if (!writeWithResponse) {
        sendCommand(data);
        return;
    }

    Request request;
    request.payload = data;
    request.command = ATT_OP_WRITE_REQUEST;
    request.reference = charHandle;
    request.reference2 = newValue;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

void QLowEnergyControllerPrivate::writeDescriptor(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle charHandle,
        const QLowEnergyHandle descriptorHandle,
        const QByteArray &newValue)
{
    Q_ASSERT(!service.isNull());

    if (newValue.size() > (mtuSize - WRITE_REQUEST_HEADER_SIZE)) {
        sendNextPrepareWriteRequest(descriptorHandle, newValue, 0);
        sendNextPendingRequest();
        return;
    }

    quint8 packet[WRITE_REQUEST_HEADER_SIZE];
    packet[0] = ATT_OP_WRITE_REQUEST;
    bt_put_unaligned(htobs(descriptorHandle), (quint16 *) &packet[1]);

    const int size = WRITE_REQUEST_HEADER_SIZE + newValue.size();
    QByteArray data(size, Qt::Uninitialized);
    memcpy(data.data(), packet, WRITE_REQUEST_HEADER_SIZE);
    memcpy(&(data.data()[WRITE_REQUEST_HEADER_SIZE]), newValue.constData(), newValue.size());

    qCDebug(QT_BT_BLUEZ) << "Writing descriptor" << hex << descriptorHandle
                         << "(size:" << size << ")";

    Request request;
    request.payload = data;
    request.command = ATT_OP_WRITE_REQUEST;
    request.reference = (charHandle | (descriptorHandle << 16));
    request.reference2 = newValue;
    openRequests.enqueue(request);

    sendNextPendingRequest();
}

/*!
 * Returns true if the encryption change was successfully requested.
 * The request is triggered if we got a related ATT error.
 */
bool QLowEnergyControllerPrivate::increaseEncryptLevelfRequired(quint8 errorCode)
{
    if (securityLevelValue == BT_SECURITY_HIGH)
        return false;

    switch (errorCode) {
    case ATT_ERROR_INSUF_AUTHORIZATION:
    case ATT_ERROR_INSUF_ENCRYPTION:
    case ATT_ERROR_INSUF_AUTHENTICATION:
        if (!hciManager->isValid())
            return false;
        if (!hciManager->monitorEvent(HciManager::EncryptChangeEvent))
            return false;
        if (securityLevelValue != BT_SECURITY_HIGH) {
            qCDebug(QT_BT_BLUEZ) << "Requesting encrypted link";
            if (setSecurityLevel(BT_SECURITY_HIGH))
                return true;
        }
        break;
    default:
        break;
    }

    return false;
}

QT_END_NAMESPACE
