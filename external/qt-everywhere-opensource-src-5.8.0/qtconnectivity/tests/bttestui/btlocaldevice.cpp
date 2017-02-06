/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "btlocaldevice.h"
#include <QDebug>
#include <QTimer>
#include <QtBluetooth/QBluetoothServiceInfo>

#define BTCHAT_DEVICE_ADDR "00:15:83:38:17:C3"

//same uuid as examples/bluetooth/btchat
#define TEST_SERVICE_UUID "e8e10f95-1a70-4b27-9ccf-02010264e9c8"

#define SOCKET_PROTOCOL QBluetoothServiceInfo::RfcommProtocol
//#define SOCKET_PROTOCOL QBluetoothServiceInfo::L2capProtocol

BtLocalDevice::BtLocalDevice(QObject *parent) :
    QObject(parent), securityFlags(QBluetooth::NoSecurity)
{
    localDevice = new QBluetoothLocalDevice(this);
    connect(localDevice, SIGNAL(error(QBluetoothLocalDevice::Error)),
            this, SIGNAL(error(QBluetoothLocalDevice::Error)));
    connect(localDevice, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)),
            this, SIGNAL(hostModeStateChanged()));
    connect(localDevice, SIGNAL(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)),
            this, SLOT(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)));
    connect(localDevice, SIGNAL(deviceConnected(QBluetoothAddress)),
            this, SLOT(connected(QBluetoothAddress)));
    connect(localDevice, SIGNAL(deviceDisconnected(QBluetoothAddress)),
            this, SLOT(disconnected(QBluetoothAddress)));
    connect(localDevice, SIGNAL(pairingDisplayConfirmation(QBluetoothAddress,QString)),
            this, SLOT(pairingDisplayConfirmation(QBluetoothAddress,QString)));

    if (localDevice->isValid()) {
        deviceAgent = new QBluetoothDeviceDiscoveryAgent(this);
        connect(deviceAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
                this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));
        connect(deviceAgent, SIGNAL(finished()),
                this, SLOT(discoveryFinished()));
        connect(deviceAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
                this, SLOT(discoveryError(QBluetoothDeviceDiscoveryAgent::Error)));
        connect(deviceAgent, SIGNAL(canceled()),
                this, SLOT(discoveryCanceled()));

        serviceAgent = new QBluetoothServiceDiscoveryAgent(this);
        connect(serviceAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
                this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
        connect(serviceAgent, SIGNAL(finished()),
                this, SLOT(serviceDiscoveryFinished()));
        connect(serviceAgent, SIGNAL(canceled()),
                this, SLOT(serviceDiscoveryCanceled()));
        connect(serviceAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)),
                this, SLOT(serviceDiscoveryError(QBluetoothServiceDiscoveryAgent::Error)));

        socket = new QBluetoothSocket(SOCKET_PROTOCOL, this);
        connect(socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)),
                this, SLOT(socketStateChanged(QBluetoothSocket::SocketState)));
        connect(socket, SIGNAL(error(QBluetoothSocket::SocketError)),
                this, SLOT(socketError(QBluetoothSocket::SocketError)));
        connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
        connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
        setSecFlags(socket->preferredSecurityFlags());

        server = new QBluetoothServer(SOCKET_PROTOCOL, this);
        connect(server, SIGNAL(newConnection()), this, SLOT(serverNewConnection()));
        connect(server, SIGNAL(error(QBluetoothServer::Error)),
                this, SLOT(serverError(QBluetoothServer::Error)));
    } else {
        deviceAgent = 0;
        serviceAgent = 0;
        socket = 0;
        server = 0;
    }
}

BtLocalDevice::~BtLocalDevice()
{
    while (!serverSockets.isEmpty())
    {
        QBluetoothSocket* s = serverSockets.takeFirst();
        s->abort();
        s->deleteLater();
    }
}

int BtLocalDevice::secFlags() const
{
    return (int)securityFlags;
}

void BtLocalDevice::setSecFlags(int newFlags)
{
    QBluetooth::SecurityFlags fl(newFlags);

    if (securityFlags != fl) {
        securityFlags = fl;
        emit secFlagsChanged();
    }
}

QString BtLocalDevice::hostMode() const
{
    switch (localDevice->hostMode()) {
    case QBluetoothLocalDevice::HostDiscoverable:
        return QStringLiteral("HostMode: Discoverable");
    case QBluetoothLocalDevice::HostConnectable:
        return QStringLiteral("HostMode: Connectable");
    case QBluetoothLocalDevice::HostDiscoverableLimitedInquiry:
        return QStringLiteral("HostMode: DiscoverableLimit");
    case QBluetoothLocalDevice::HostPoweredOff:
        return QStringLiteral("HostMode: Powered Off");
    }

    return QStringLiteral("HostMode: <None>");
}

void BtLocalDevice::setHostMode(int newMode)
{
    localDevice->setHostMode((QBluetoothLocalDevice::HostMode)newMode);
}

void BtLocalDevice::requestPairingUpdate(bool isPairing)
{
    QBluetoothAddress baddr(BTCHAT_DEVICE_ADDR);
    if (baddr.isNull())
        return;



    if (isPairing) {
        //toggle between authorized and non-authorized pairing to achieve better
        //level of testing
        static short pairing = 0;
        if ((pairing%2) == 1)
            localDevice->requestPairing(baddr, QBluetoothLocalDevice::Paired);
        else
            localDevice->requestPairing(baddr, QBluetoothLocalDevice::AuthorizedPaired);
        pairing++;
    } else {
        localDevice->requestPairing(baddr, QBluetoothLocalDevice::Unpaired);
    }

    for (int i = 0; i < foundTestServers.count(); i++) {
        if (isPairing)
            localDevice->requestPairing(foundTestServers.at(i).device().address(),
                                    QBluetoothLocalDevice::Paired);
        else
            localDevice->requestPairing(foundTestServers.at(i).device().address(),
                                    QBluetoothLocalDevice::Unpaired);
    }
}

void BtLocalDevice::pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
    qDebug() << "(Un)Pairing finished" << address.toString() << pairing;
}

void BtLocalDevice::connected(const QBluetoothAddress &addr)
{
    qDebug() << "Newly connected device" << addr.toString();
}

void BtLocalDevice::disconnected(const QBluetoothAddress &addr)
{
    qDebug() << "Newly disconnected device" << addr.toString();
}

void BtLocalDevice::pairingDisplayConfirmation(const QBluetoothAddress &address, const QString &pin)
{
    Q_UNUSED(pin);
    Q_UNUSED(address);
    QTimer::singleShot(3000, this, SLOT(confirmPairing()));
}

void BtLocalDevice::confirmPairing()
{
    static bool confirm = false;
    confirm = !confirm; //toggle
    qDebug() << "######" << "Sending pairing confirmation: " << confirm;
    localDevice->pairingConfirmation(confirm);
}

void BtLocalDevice::cycleSecurityFlags()
{
    if (securityFlags.testFlag(QBluetooth::Secure))
        setSecFlags(QBluetooth::NoSecurity);
    else if (securityFlags.testFlag(QBluetooth::Encryption))
        setSecFlags(secFlags() | QBluetooth::Secure);
    else if (securityFlags.testFlag(QBluetooth::Authentication))
        setSecFlags(secFlags() | QBluetooth::Encryption);
    else if (securityFlags.testFlag(QBluetooth::Authorization))
        setSecFlags(secFlags() | QBluetooth::Authentication);
    else
        setSecFlags(secFlags() | QBluetooth::Authorization);
}

void BtLocalDevice::deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    QString services;
    if (info.serviceClasses() & QBluetoothDeviceInfo::PositioningService)
        services += "Position|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::NetworkingService)
        services += "Network|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::RenderingService)
        services += "Rendering|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::CapturingService)
        services += "Capturing|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::ObjectTransferService)
        services += "ObjectTra|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::AudioService)
        services += "Audio|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::TelephonyService)
        services += "Telephony|";
    if (info.serviceClasses() & QBluetoothDeviceInfo::InformationService)
        services += "Information|";

    services.truncate(services.length()-1); //cut last '/'

    qDebug() << "Found new device: " << info.name() << info.isValid() << info.address().toString()
                                     << info.rssi() << info.majorDeviceClass()
                                     << info.minorDeviceClass() << services;

}

void BtLocalDevice::discoveryFinished()
{
    qDebug() << "###### Device Discovery Finished";
}

void BtLocalDevice::discoveryCanceled()
{
    qDebug() << "###### Device Discovery Canceled";
}

void BtLocalDevice::discoveryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QBluetoothDeviceDiscoveryAgent *client = qobject_cast<QBluetoothDeviceDiscoveryAgent *>(sender());
    if (!client)
        return;
    qDebug() << "###### Device Discovery Error:" << error << (client ? client->errorString() : QString());
}

void BtLocalDevice::startDiscovery()
{
    if (deviceAgent) {
        qDebug() << "###### Starting device discovery process";
        deviceAgent->start();
    }
}

void BtLocalDevice::stopDiscovery()
{
    if (deviceAgent) {
        qDebug() << "Stopping device discovery process";
        deviceAgent->stop();
    }
}

void BtLocalDevice::startServiceDiscovery(bool isMinimalDiscovery)
{
    if (serviceAgent) {
        serviceAgent->setRemoteAddress(QBluetoothAddress());

        qDebug() << "###### Starting service discovery process";
        serviceAgent->start(isMinimalDiscovery
                            ? QBluetoothServiceDiscoveryAgent::MinimalDiscovery
                            : QBluetoothServiceDiscoveryAgent::FullDiscovery);
    }
}

void BtLocalDevice::startTargettedServiceDiscovery()
{
    if (serviceAgent) {
        const QBluetoothAddress baddr(BTCHAT_DEVICE_ADDR);
        qDebug() << "###### Starting service discovery on"
                 << baddr.toString();
        if (baddr.isNull())
            return;

        if (!serviceAgent->setRemoteAddress(baddr)) {
            qWarning() << "###### Cannot set remote address. Aborting";
            return;
        }

        serviceAgent->start();
    }
}

void BtLocalDevice::stopServiceDiscovery()
{
    if (serviceAgent) {
        qDebug() << "Stopping service discovery process";
        serviceAgent->stop();
    }
}

void BtLocalDevice::serviceDiscovered(const QBluetoothServiceInfo &info)
{
    QStringList classIds;
    foreach (const QBluetoothUuid &uuid, info.serviceClassUuids())
        classIds.append(uuid.toString());
    qDebug() << "$$ Found new service" << info.device().address().toString()
             << info.serviceUuid() << info.serviceName() << info.serviceDescription() << classIds;

    if (info.serviceUuid() == QBluetoothUuid(QString(TEST_SERVICE_UUID))
            || info.serviceClassUuids().contains(QBluetoothUuid(QString(TEST_SERVICE_UUID))))
    {
        //This is here to detect the test server for SPP testing later on
        bool alreadyKnown = false;
        foreach (const QBluetoothServiceInfo& found, foundTestServers) {
            if (found.device().address() == info.device().address()) {
                alreadyKnown = true;
                break;
            }
        }

        if (!alreadyKnown) {
            foundTestServers.append(info);
            qDebug() << "@@@@@@@@ Adding:" << info.device().address().toString();
        }
    }
}

void BtLocalDevice::serviceDiscoveryFinished()
{
    qDebug() << "###### Service Discovery Finished";
}

void BtLocalDevice::serviceDiscoveryCanceled()
{
    qDebug() << "###### Service Discovery Canceled";
}

void BtLocalDevice::serviceDiscoveryError(QBluetoothServiceDiscoveryAgent::Error error)
{
    QBluetoothServiceDiscoveryAgent *client = qobject_cast<QBluetoothServiceDiscoveryAgent *>(sender());
    if (!client)
        return;
    qDebug() << "###### Service Discovery Error:" << error << (client ? client->errorString() : QString());
}

void BtLocalDevice::dumpServiceDiscovery()
{
    if (deviceAgent) {
        qDebug() << "Device Discovery active:" << deviceAgent->isActive();
        qDebug() << "Error:" << deviceAgent->error() << deviceAgent->errorString();
        QList<QBluetoothDeviceInfo> list = deviceAgent->discoveredDevices();
        qDebug() << "Discovered Devices:" << list.count();

        foreach (const QBluetoothDeviceInfo &info, list)
            qDebug() << info.name() << info.address().toString() << info.rssi();
    }
    if (serviceAgent) {
        qDebug() << "Service Discovery active:" << serviceAgent->isActive();
        qDebug() << "Error:" << serviceAgent->error() << serviceAgent->errorString();
        QList<QBluetoothServiceInfo> list = serviceAgent->discoveredServices();
        qDebug() << "Discovered Services:" << list.count();

        foreach (const QBluetoothServiceInfo &i, list) {
            qDebug() << i.device().address().toString() << i.device().name() << i.serviceName();
        }

        qDebug() << "###### TestServer offered by:";
        foreach (const QBluetoothServiceInfo& found, foundTestServers) {
            qDebug() << found.device().name() << found.device().address().toString();
        }
    }
}

void BtLocalDevice::connectToService()
{
    if (socket) {
        if (socket->preferredSecurityFlags() != securityFlags)
            socket->setPreferredSecurityFlags(securityFlags);
        socket->connectToService(QBluetoothAddress(BTCHAT_DEVICE_ADDR),QBluetoothUuid(QString(TEST_SERVICE_UUID)));
    }
}

void BtLocalDevice::connectToServiceViaSearch()
{
    if (socket) {
        qDebug() << "###### Connecting to service socket";
        if (!foundTestServers.isEmpty()) {
            if (socket->preferredSecurityFlags() != securityFlags)
                socket->setPreferredSecurityFlags(securityFlags);

            QBluetoothServiceInfo info = foundTestServers.at(0);
            socket->connectToService(info);
        } else {
            qWarning() << "Perform search for test service before triggering this function";
        }
    }
}

void BtLocalDevice::disconnectToService()
{
    if (socket) {
        qDebug() << "###### Disconnecting socket";
        socket->disconnectFromService();
    }
}

void BtLocalDevice::closeSocket()
{
    if (socket) {
        qDebug() << "###### Closing socket";
        socket->close();
    }

    if (!serverSockets.isEmpty()) {
        qDebug() << "###### Closing server sockets";
        foreach (QBluetoothSocket *s, serverSockets)
            s->close();
    }
}

void BtLocalDevice::abortSocket()
{
    if (socket) {
        qDebug() << "###### Disconnecting socket";
        socket->abort();
    }

    if (!serverSockets.isEmpty()) {
        qDebug() << "###### Closing server sockets";
        foreach (QBluetoothSocket *s, serverSockets)
            s->abort();
    }
}

void BtLocalDevice::socketConnected()
{
    qDebug() << "###### Socket connected";
}

void BtLocalDevice::socketDisconnected()
{
    qDebug() << "###### Socket disconnected";
}

void BtLocalDevice::socketError(QBluetoothSocket::SocketError error)
{
    QBluetoothSocket *client = qobject_cast<QBluetoothSocket *>(sender());

    qDebug() << "###### Socket error" << error << (client ? client->errorString() : QString());
}

void BtLocalDevice::socketStateChanged(QBluetoothSocket::SocketState state)
{
    qDebug() << "###### Socket state" << state;
    emit socketStateUpdate((int) state);
}

void BtLocalDevice::dumpSocketInformation()
{
    if (socket) {
        qDebug() << "*******************************";
        qDebug() << "Local info (addr, name, port):" << socket->localAddress().toString()
                 << socket->localName() << socket->localPort();
        qDebug() << "Peer Info (adr, name, port):" << socket->peerAddress().toString()
                 << socket->peerName() << socket->peerPort();
        qDebug() << "socket type:" << socket->socketType();
        qDebug() << "socket state:" << socket->state();
        qDebug() << "socket bytesAvailable()" << socket->bytesAvailable();
        QString tmp;
        switch (socket->error()) {
            case QBluetoothSocket::NoSocketError: tmp += "NoSocketError"; break;
            case QBluetoothSocket::UnknownSocketError: tmp += "UnknownSocketError"; break;
            case QBluetoothSocket::HostNotFoundError: tmp += "HostNotFoundError"; break;
            case QBluetoothSocket::ServiceNotFoundError: tmp += "ServiceNotFound"; break;
            case QBluetoothSocket::NetworkError: tmp += "NetworkError"; break;
            //case QBluetoothSocket::OperationError: tmp+= "OperationError"; break;
            case QBluetoothSocket::UnsupportedProtocolError: tmp += "UnsupportedProtocolError"; break;
            default: tmp+= "Undefined"; break;
        }

        qDebug() << "socket error:" << tmp << socket->errorString();
    } else {
        qDebug() << "No valid socket existing";
    }
}

void BtLocalDevice::writeData()
{
    const char * testData = "ABCABC\n";
    if (socket && socket->state() == QBluetoothSocket::ConnectedState) {
        socket->write(testData);
    }
    foreach (QBluetoothSocket* client, serverSockets) {
        client->write(testData);
    }
}

void BtLocalDevice::readData()
{
    if (socket) {
        while (socket->canReadLine()) {
            QByteArray line = socket->readLine().trimmed();
            qDebug() << ">> peer(" << socket->peerName() << socket->peerAddress()
                     << socket->peerPort() << ") local("
                     << socket->localName() << socket->localAddress() << socket->localPort()
                     << ")>>" << QString::fromUtf8(line.constData(), line.length());
        }
    }
}

void BtLocalDevice::serverError(QBluetoothServer::Error error)
{
    qDebug() << "###### Server socket error" << error;
}

void BtLocalDevice::serverListenPort()
{
    if (server && localDevice) {
        if (server->isListening() || serviceInfo.isRegistered()) {
            qDebug() << "###### Already listening" << serviceInfo.isRegistered();
            return;
        }

        if (server->securityFlags() != securityFlags) {
            qDebug() << "###### Setting security policy on server socket" << securityFlags;
            server->setSecurityFlags(securityFlags);
        }

        qDebug() << "###### Start listening via port";
        bool ret = server->listen(localDevice->address());
        qDebug() << "###### Listening(Expecting TRUE):" << ret;

        if (!ret)
            return;

        QBluetoothServiceInfo::Sequence classId;
        classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
        serviceInfo.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList,
                                 classId);

        classId.prepend(QVariant::fromValue(QBluetoothUuid(QString(TEST_SERVICE_UUID))));
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

        // Service name, description and provider
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, tr("Bt Chat Server"));
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                                 tr("Example bluetooth chat server"));
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceProvider, tr("qt-project.org"));

        // Service UUID set
        serviceInfo.setServiceUuid(QBluetoothUuid(QString(TEST_SERVICE_UUID)));


        // Service Discoverability
        serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                                 QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

        // Protocol descriptor list
        QBluetoothServiceInfo::Sequence protocolDescriptorList;
        QBluetoothServiceInfo::Sequence protocol;
        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
        if (server->serverType() == QBluetoothServiceInfo::L2capProtocol)
            protocol << QVariant::fromValue(server->serverPort());
        protocolDescriptorList.append(QVariant::fromValue(protocol));

        if (server->serverType() == QBluetoothServiceInfo::RfcommProtocol) {
            protocol.clear();
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                     << QVariant::fromValue(quint8(server->serverPort()));
            protocolDescriptorList.append(QVariant::fromValue(protocol));
        }
        serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                                 protocolDescriptorList);

        //Register service
        qDebug() << "###### Registering service on" << localDevice->address().toString() << server->serverPort();
        bool result = serviceInfo.registerService(localDevice->address());
        if (!result) {
            server->close();
            qDebug() << "###### Reverting registration due to SDP failure.";
        }
    }

}

void BtLocalDevice::serverListenUuid()
{
    if (server) {
        if (server->isListening() || serviceInfo.isRegistered()) {
            qDebug() << "###### Already listening" << serviceInfo.isRegistered();
            return;
        }

        if (server->securityFlags() != securityFlags) {
            qDebug() << "###### Setting security policy on server socket" << securityFlags;
            server->setSecurityFlags(securityFlags);
        }

        qDebug() << "###### Start listening via UUID";
        serviceInfo = server->listen(QBluetoothUuid(QString(TEST_SERVICE_UUID)), tr("Bt Chat Server"));
        qDebug() << "###### Listening(Expecting TRUE, TRUE):" << serviceInfo.isRegistered() << serviceInfo.isValid();
    }
}

void BtLocalDevice::serverClose()
{
    if (server) {
        qDebug() << "###### Closing Server socket";
        if (serviceInfo.isRegistered())
            serviceInfo.unregisterService();
        server->close();
    }
}

void BtLocalDevice::serverNewConnection()
{
    qDebug() << "###### New incoming server connection, pending:" << server->hasPendingConnections();
    if (!server->hasPendingConnections()) {
        qDebug() << "FAIL: expected pending server connection";
        return;
    }
    QBluetoothSocket *client = server->nextPendingConnection();
    if (!client) {
        qDebug() << "FAIL: Cannot obtain pending server connection";
        return;
    }

    client->setParent(this);
    connect(client, SIGNAL(disconnected()), this, SLOT(clientSocketDisconnected()));
    connect(client, SIGNAL(readyRead()), this, SLOT(clientSocketReadyRead()));
    connect(client, SIGNAL(stateChanged(QBluetoothSocket::SocketState)),
            this, SLOT(socketStateChanged(QBluetoothSocket::SocketState)));
    connect(client, SIGNAL(error(QBluetoothSocket::SocketError)),
            this, SLOT(socketError(QBluetoothSocket::SocketError)));
    connect(client, SIGNAL(connected()), this, SLOT(socketConnected()));
    serverSockets.append(client);
}

void BtLocalDevice::clientSocketDisconnected()
{
    QBluetoothSocket *client = qobject_cast<QBluetoothSocket *>(sender());
    if (!client)
        return;

    qDebug() << "######" << "Removing server socket connection";

    serverSockets.removeOne(client);
    client->deleteLater();
}


void BtLocalDevice::clientSocketReadyRead()
{
    QBluetoothSocket *socket = qobject_cast<QBluetoothSocket *>(sender());
    if (!socket)
        return;

    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine().trimmed();
        QString lineString = QString::fromUtf8(line.constData(), line.length());
        qDebug() <<  ">>(" << server->serverAddress() << server->serverPort()  <<")>>"
                 << lineString;

        //when using the tst_QBluetoothSocket we echo received text back
        //Any line starting with "Echo:" will be echoed
        if (lineString.startsWith(QStringLiteral("Echo:"))) {
            qDebug() << "Assuming tst_qbluetoothsocket as client. Echoing back.";
            lineString += QLatin1Char('\n');
            socket->write(lineString.toUtf8());
        }
    }
}


void BtLocalDevice::dumpServerInformation()
{
    static QBluetooth::SecurityFlags secFlag = QBluetooth::Authentication;
    if (server) {
        qDebug() << "*******************************";
        qDebug() << "server port:" <<server->serverPort()
                 << "type:" << server->serverType()
                 << "address:" << server->serverAddress().toString();
        qDebug() << "error:" << server->error();
        qDebug() << "listening:" << server->isListening()
                 << "hasPending:" << server->hasPendingConnections()
                 << "maxPending:" << server->maxPendingConnections();
        qDebug() << "security:" << server->securityFlags() << "Togling security flag";
        if (secFlag == QBluetooth::Authentication)
            secFlag = QBluetooth::Encryption;
        else
            secFlag = QBluetooth::Authentication;

        //server->setSecurityFlags(secFlag);

        foreach (const QBluetoothSocket *client, serverSockets) {
            qDebug() << "##" << client->localAddress().toString()
                     << client->localName() << client->localPort();
            qDebug() << "##" << client->peerAddress().toString()
                     << client->peerName() << client->peerPort();
            qDebug() << client->socketType() << client->state();
            qDebug() << "Pending bytes: " << client->bytesAvailable();
            QString tmp;
            switch (client->error()) {
            case QBluetoothSocket::NoSocketError: tmp += "NoSocketError"; break;
            case QBluetoothSocket::UnknownSocketError: tmp += "UnknownSocketError"; break;
            case QBluetoothSocket::HostNotFoundError: tmp += "HostNotFoundError"; break;
            case QBluetoothSocket::ServiceNotFoundError: tmp += "ServiceNotFound"; break;
            case QBluetoothSocket::NetworkError: tmp += "NetworkError"; break;
            case QBluetoothSocket::UnsupportedProtocolError: tmp += "UnsupportedProtocolError"; break;
            //case QBluetoothSocket::OperationError: tmp+= "OperationError"; break;
            default: tmp += QString::number((int)client->error()); break;
            }

            qDebug() << "socket error:" << tmp << client->errorString();
        }
    }
}

void BtLocalDevice::dumpInformation()
{
    qDebug() << "###### default local device";
    dumpLocalDevice(localDevice);
    QList<QBluetoothHostInfo> list = QBluetoothLocalDevice::allDevices();
    qDebug() << "Found local devices: "  << list.count();
    foreach (const QBluetoothHostInfo &info, list) {
        qDebug() << "    " << info.address().toString() << " " <<info.name();
    }

    QBluetoothAddress address(QStringLiteral("11:22:33:44:55:66"));
    QBluetoothLocalDevice temp(address);
    qDebug() << "###### 11:22:33:44:55:66 address valid:" << !address.isNull();
    dumpLocalDevice(&temp);

    QBluetoothAddress address2;
    QBluetoothLocalDevice temp2(address2);
    qDebug() << "###### 00:00:00:00:00:00 address valid:" << !address2.isNull();
    dumpLocalDevice(&temp2);

    const QBluetoothAddress BB(BTCHAT_DEVICE_ADDR);
    qDebug() << "###### Bonding state with" <<  QString(BTCHAT_DEVICE_ADDR) << ":" << localDevice->pairingStatus(BB);
    qDebug() << "###### Bonding state with" << address2.toString() << ": " << localDevice->pairingStatus(address2);
    qDebug() << "###### Bonding state with" << address.toString() << ": " << localDevice->pairingStatus(address);

    qDebug() << "###### Connected Devices";
    foreach (const QBluetoothAddress &addr, localDevice->connectedDevices())
        qDebug() << "    " << addr.toString();

    qDebug() << "###### Discovered Devices";
    if (deviceAgent)
        foreach (const QBluetoothDeviceInfo &info, deviceAgent->discoveredDevices())
            deviceDiscovered(info);

    QBluetoothDeviceDiscoveryAgent invalidAgent(QBluetoothAddress("11:22:33:44:55:66"));
    invalidAgent.start();
    qDebug() << "######" << "Testing device discovery agent constructor with invalid address";
    qDebug() << "######" << (invalidAgent.error() == QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError)
                         << "(Expected: true)";
    QBluetoothDeviceDiscoveryAgent validAgent(localDevice->address());
    validAgent.start();
    qDebug() << "######" << (validAgent.error() == QBluetoothDeviceDiscoveryAgent::NoError) << "(Expected: true)";

    QBluetoothServiceDiscoveryAgent invalidSAgent(QBluetoothAddress("11:22:33:44:55:66"));
    invalidSAgent.start();
    qDebug() << "######" << "Testing service discovery agent constructor with invalid address";
    qDebug() << "######" << (invalidSAgent.error() == QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError)
                         << "(Expected: true)";
    QBluetoothServiceDiscoveryAgent validSAgent(localDevice->address());
    validSAgent.start();
    qDebug() << "######" << (validSAgent.error() == QBluetoothServiceDiscoveryAgent::NoError) << "(Expected: true)";
}

void BtLocalDevice::powerOn()
{
    qDebug() << "Powering on";
    localDevice->powerOn();
}

void BtLocalDevice::reset()
{
    emit error((QBluetoothLocalDevice::Error)1000);
    if (serviceAgent) {
        serviceAgent->clear();
    }
    foundTestServers.clear();
}

void BtLocalDevice::dumpLocalDevice(QBluetoothLocalDevice *dev)
{
    qDebug() << "    Valid: " << dev->isValid();
    qDebug() << "    Name" << dev->name();
    qDebug() << "    Address" << dev->address().toString();
    qDebug() << "    HostMode" << dev->hostMode();
}
