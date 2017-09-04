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

#ifndef BTLOCALDEVICE_H
#define BTLOCALDEVICE_H

#include <QtCore/QObject>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QBluetoothLocalDevice>
#include <QtBluetooth/QBluetoothServer>
#include <QtBluetooth/QBluetoothServiceDiscoveryAgent>
#include <QtBluetooth/QBluetoothSocket>

class BtLocalDevice : public QObject
{
    Q_OBJECT
public:
    explicit BtLocalDevice(QObject *parent = 0);
    ~BtLocalDevice();
    Q_PROPERTY(QString hostMode READ hostMode NOTIFY hostModeStateChanged)
    Q_PROPERTY(int secFlags READ secFlags WRITE setSecFlags
               NOTIFY secFlagsChanged)

    int secFlags() const;
    void setSecFlags(int);
    QString hostMode() const;

signals:
    void error(QBluetoothLocalDevice::Error error);
    void hostModeStateChanged();
    void socketStateUpdate(int foobar);
    void secFlagsChanged();

public slots:
    //QBluetoothLocalDevice
    void dumpInformation();
    void powerOn();
    void reset();
    void setHostMode(int newMode);
    void requestPairingUpdate(bool isPairing);
    void pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing);
    void connected(const QBluetoothAddress &addr);
    void disconnected(const QBluetoothAddress &addr);
    void pairingDisplayConfirmation(const QBluetoothAddress &address, const QString &pin);
    void confirmPairing();
    void cycleSecurityFlags();

    //QBluetoothDeviceDiscoveryAgent
    void deviceDiscovered(const QBluetoothDeviceInfo &info);
    void discoveryFinished();
    void discoveryCanceled();
    void discoveryError(QBluetoothDeviceDiscoveryAgent::Error error);
    void startDiscovery();
    void stopDiscovery();

    //QBluetoothServiceDiscoveryAgent
    void startServiceDiscovery(bool isMinimalDiscovery);
    void startTargettedServiceDiscovery();
    void stopServiceDiscovery();
    void serviceDiscovered(const QBluetoothServiceInfo &info);
    void serviceDiscoveryFinished();
    void serviceDiscoveryCanceled();
    void serviceDiscoveryError(QBluetoothServiceDiscoveryAgent::Error error);
    void dumpServiceDiscovery();

    //QBluetoothSocket
    void connectToService();
    void connectToServiceViaSearch();
    void disconnectToService();
    void closeSocket();
    void abortSocket();
    void socketConnected();
    void socketDisconnected();
    void socketError(QBluetoothSocket::SocketError error);
    void socketStateChanged(QBluetoothSocket::SocketState);
    void dumpSocketInformation();
    void writeData();
    void readData();

    //QBluetoothServer
    void serverError(QBluetoothServer::Error error);
    void serverListenPort();
    void serverListenUuid();
    void serverClose();
    void serverNewConnection();
    void clientSocketDisconnected();
    void clientSocketReadyRead();
    void dumpServerInformation();

private:
    void dumpLocalDevice(QBluetoothLocalDevice *dev);

    QBluetoothLocalDevice *localDevice;
    QBluetoothDeviceDiscoveryAgent *deviceAgent;
    QBluetoothServiceDiscoveryAgent *serviceAgent;
    QBluetoothSocket *socket;
    QBluetoothServer *server;
    QList<QBluetoothSocket *> serverSockets;
    QBluetoothServiceInfo serviceInfo;

    QList<QBluetoothServiceInfo> foundTestServers;
    QBluetooth::SecurityFlags securityFlags;
};

#endif // BTLOCALDEVICE_H
