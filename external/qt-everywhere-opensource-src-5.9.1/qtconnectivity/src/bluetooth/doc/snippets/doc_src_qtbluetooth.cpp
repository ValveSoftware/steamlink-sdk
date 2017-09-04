/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

//! [include]
#include <QtBluetooth/QBluetoothLocalDevice>
//! [include]
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtBluetooth/QBluetoothDeviceDiscoveryAgent>
#include <QtBluetooth/QBluetoothServiceDiscoveryAgent>
#include <QtBluetooth/QBluetoothTransferManager>
#include <QtBluetooth/QBluetoothTransferRequest>
#include <QtBluetooth/QBluetoothTransferReply>

#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QLowEnergyCharacteristic>

//! [namespace]
QT_USE_NAMESPACE
//! [namespace]

class MyClass : public QObject
{
    Q_OBJECT
public:
    MyClass() : QObject() {}
    void localDevice();
    void startDeviceDiscovery();
    void startServiceDiscovery();
    void objectPush();
    void btleSharedData();
    void enableCharNotifications();

public slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void serviceDiscovered(const QBluetoothServiceInfo &service);
    void transferFinished(QBluetoothTransferReply* reply);
    void error(QBluetoothTransferReply::TransferError errorType);
    void characteristicChanged(const QLowEnergyCharacteristic& ,const QByteArray&);
};

void MyClass::localDevice() {
//! [turningon]
QBluetoothLocalDevice localDevice;
QString localDeviceName;

// Check if Bluetooth is available on this device
if (localDevice.isValid()) {

    // Turn Bluetooth on
    localDevice.powerOn();

    // Read local device name
    localDeviceName = localDevice.name();

    // Make it visible to others
    localDevice.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

    // Get connected devices
    QList<QBluetoothAddress> remotes;
    remotes = localDevice.connectedDevices();
}
//! [turningon]


}

//! [device_discovery]
void MyClass::startDeviceDiscovery()
{

    // Create a discovery agent and connect to its signals
    QBluetoothDeviceDiscoveryAgent *discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));

    // Start a discovery
    discoveryAgent->start();

    //...
}

// In your local slot, read information about the found devices
void MyClass::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    qDebug() << "Found new device:" << device.name() << '(' << device.address().toString() << ')';
}
//! [device_discovery]

//! [service_discovery]
void MyClass::startServiceDiscovery()
{

    // Create a discovery agent and connect to its signals
    QBluetoothServiceDiscoveryAgent *discoveryAgent = new QBluetoothServiceDiscoveryAgent(this);
    connect(discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));

    // Start a discovery
    discoveryAgent->start();

    //...
}

// In your local slot, read information about the found devices
void MyClass::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    qDebug() << "Found new service:" << service.serviceName()
             << '(' << service.device().address().toString() << ')';
}
//! [service_discovery]

void MyClass::objectPush()
{
//! [sendfile]
// Create a transfer manager
QBluetoothTransferManager *transferManager = new QBluetoothTransferManager(this);

// Create the transfer request and file to be sent
QBluetoothAddress remoteAddress("00:11:22:33:44:55:66");
QBluetoothTransferRequest request(remoteAddress);
QFile *file = new QFile("testfile.txt");

// Ask the transfer manager to send it
QBluetoothTransferReply *reply = transferManager->put(request, file);
if (reply->error() == QBluetoothTransferReply::NoError) {

    // Connect to the reply's signals to be informed about the status and do cleanups when done
    QObject::connect(reply, SIGNAL(finished(QBluetoothTransferReply*)),
                     this, SLOT(transferFinished(QBluetoothTransferReply*)));
    QObject::connect(reply, SIGNAL(error(QBluetoothTransferReply::TransferError)),
                     this, SLOT(error(QBluetoothTransferReply::TransferError)));
} else {
    qWarning() << "Cannot push testfile.txt:" << reply->errorString();
}
//! [sendfile]
}

void MyClass::transferFinished(QBluetoothTransferReply* /*reply*/)
{
}

void MyClass::error(QBluetoothTransferReply::TransferError /*errorType*/)
{
}

void MyClass::characteristicChanged(const QLowEnergyCharacteristic &, const QByteArray &)
{
}

void MyClass::btleSharedData()
{
    QBluetoothDeviceInfo remoteDevice;

//! [data_share_qlowenergyservice]
    QLowEnergyService *first, *second;
    QLowEnergyController control(remoteDevice);
    control.connectToDevice();

    // waiting for connection

    first = control.createServiceObject(QBluetoothUuid::BatteryService);
    second = control.createServiceObject(QBluetoothUuid::BatteryService);
    Q_ASSERT(first->state() == QLowEnergyService::DiscoveryRequired);
    Q_ASSERT(first->state() == second->state());

    first->discoverDetails();

    Q_ASSERT(first->state() == QLowEnergyService::DiscoveringServices);
    Q_ASSERT(first->state() == second->state());
//! [data_share_qlowenergyservice]
}

void MyClass::enableCharNotifications()
{
    QBluetoothDeviceInfo remoteDevice;
    QLowEnergyService *service;
    QLowEnergyController *control = new QLowEnergyController(remoteDevice, this);
    control->connectToDevice();


    service = control->createServiceObject(QBluetoothUuid::BatteryService, this);
    if (!service)
        return;

    service->discoverDetails();

    //... wait until discovered

//! [enable_btle_notifications]
    //PreCondition: service details already discovered
    QLowEnergyCharacteristic batteryLevel = service->characteristic(
                QBluetoothUuid::BatteryLevel);
    if (!batteryLevel.isValid())
        return;

    QLowEnergyDescriptor notification = batteryLevel.descriptor(
                QBluetoothUuid::ClientCharacteristicConfiguration);
    if (!notification.isValid())
        return;

    // establish hook into notifications
    connect(service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)),
            this, SLOT(characteristicChanged(QLowEnergyCharacteristic,QByteArray)));

    // enable notification
    service->writeDescriptor(notification, QByteArray::fromHex("0100"));

    // disable notification
    //service->writeDescriptor(notification, QByteArray::fromHex("0000"));

    // wait until descriptorWritten() signal is emitted
    // to confirm successful write
//! [enable_btle_notifications]
}



int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    MyClass cl;

    return app.exec();
}

#include "doc_src_qtbluetooth.moc"
