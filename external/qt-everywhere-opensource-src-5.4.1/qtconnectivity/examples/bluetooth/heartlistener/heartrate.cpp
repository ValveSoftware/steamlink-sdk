/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "heartrate.h"

HeartRate::HeartRate():
    m_currentDevice(QBluetoothDeviceInfo()), foundHeartRateService(false),
    m_max(0), m_min(0), calories(0), m_control(0), timer(0),
    m_service(0)
{
    //! [devicediscovery-1]
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

    connect(m_deviceDiscoveryAgent, SIGNAL(deviceDiscovered(const QBluetoothDeviceInfo&)),
            this, SLOT(addDevice(const QBluetoothDeviceInfo&)));
    connect(m_deviceDiscoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
            this, SLOT(deviceScanError(QBluetoothDeviceDiscoveryAgent::Error)));
    connect(m_deviceDiscoveryAgent, SIGNAL(finished()), this, SLOT(scanFinished()));
    //! [devicediscovery-1]

    // initialize random seed for demo mode
    qsrand(QTime::currentTime().msec());
}

HeartRate::~HeartRate()
{
    qDeleteAll(m_devices);
    m_devices.clear();
}

void HeartRate::deviceSearch()
{
    qDeleteAll(m_devices);
    m_devices.clear();
    //! [devicediscovery-2]
    m_deviceDiscoveryAgent->start();
    //! [devicediscovery-2]
    setMessage("Scanning for devices...");
}

//! [devicediscovery-3]
void HeartRate::addDevice(const QBluetoothDeviceInfo &device)
{
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        qWarning() << "Discovered LE Device name: " << device.name() << " Address: "
                   << device.address().toString();
//! [devicediscovery-3]
        DeviceInfo *dev = new DeviceInfo(device);
        m_devices.append(dev);
        setMessage("Low Energy device found. Scanning for more...");
//! [devicediscovery-4]
    }
    //...
}
//! [devicediscovery-4]

void HeartRate::scanFinished()
{
    if (m_devices.size() == 0)
        setMessage("No Low Energy devices found");
    Q_EMIT nameChanged();
}

void HeartRate::deviceScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError)
        setMessage("The Bluetooth adaptor is powered off, power it on before doing discovery.");
    else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError)
        setMessage("Writing or reading from the device resulted in an error.");
    else
        setMessage("An unknown error has occurred.");
}


void HeartRate::setMessage(QString message)
{
    m_info = message;
    Q_EMIT messageChanged();
}

QString HeartRate::message() const
{
    return m_info;
}

QVariant HeartRate::name()
{
    return QVariant::fromValue(m_devices);
}

void HeartRate::connectToService(const QString &address)
{
    m_measurements.clear();

    bool deviceFound = false;
    for (int i = 0; i < m_devices.size(); i++) {
        if (((DeviceInfo*)m_devices.at(i))->getAddress() == address ) {
            m_currentDevice.setDevice(((DeviceInfo*)m_devices.at(i))->getDevice());
            setMessage("Connecting to device...");
            deviceFound = true;
            break;
        }
    }
    // we are running demo mode
    if (!deviceFound) {
        startDemo();
        return;
    }

    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = 0;

    }
    //! [Connect signals]
    m_control = new QLowEnergyController(m_currentDevice.getDevice().address(),
                                            this);
    connect(m_control, SIGNAL(serviceDiscovered(QBluetoothUuid)),
            this, SLOT(serviceDiscovered(QBluetoothUuid)));
    connect(m_control, SIGNAL(discoveryFinished()),
            this, SLOT(serviceScanDone()));
    connect(m_control, SIGNAL(error(QLowEnergyController::Error)),
            this, SLOT(controllerError(QLowEnergyController::Error)));
    connect(m_control, SIGNAL(connected()),
            this, SLOT(deviceConnected()));
    connect(m_control, SIGNAL(disconnected()),
            this, SLOT(deviceDisconnected()));

    m_control->connectToDevice();
    //! [Connect signals]
}

//! [Connecting to service]

void HeartRate::deviceConnected()
{
    m_control->discoverServices();
}

void HeartRate::deviceDisconnected()
{
    setMessage("Heart Rate service disconnected");
    qWarning() << "Remote device disconnected";
}

//! [Connecting to service]

//! [Filter HeartRate service 1]
void HeartRate::serviceDiscovered(const QBluetoothUuid &gatt)
{
    if (gatt == QBluetoothUuid(QBluetoothUuid::HeartRate)) {
        setMessage("Heart Rate service discovered. Waiting for service scan to be done...");
        foundHeartRateService = true;
    }
}
//! [Filter HeartRate service 1]

void HeartRate::serviceScanDone()
{
    delete m_service;
    m_service = 0;

    //! [Filter HeartRate service 2]
    if (foundHeartRateService) {
        setMessage("Connecting to service...");
        m_service = m_control->createServiceObject(
                    QBluetoothUuid(QBluetoothUuid::HeartRate), this);
    }

    if (!m_service) {
        setMessage("Heart Rate Service not found.");
        return;
    }

    connect(m_service, SIGNAL(stateChanged(QLowEnergyService::ServiceState)),
            this, SLOT(serviceStateChanged(QLowEnergyService::ServiceState)));
    connect(m_service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)),
            this, SLOT(updateHeartRateValue(QLowEnergyCharacteristic,QByteArray)));
    connect(m_service, SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)),
            this, SLOT(confirmedDescriptorWrite(QLowEnergyDescriptor,QByteArray)));

    m_service->discoverDetails();
    //! [Filter HeartRate service 2]
}

void HeartRate::disconnectService()
{
    foundHeartRateService = false;
    m_stop = QDateTime::currentDateTime();

    if (m_devices.isEmpty()) {
        if (timer)
            timer->stop();
        return;
    }

    //disable notifications
    if (m_notificationDesc.isValid() && m_service) {
        m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0000"));
    } else {
        m_control->disconnectFromDevice();
        delete m_service;
        m_service = 0;
    }
}

//! [Error handling]
void HeartRate::controllerError(QLowEnergyController::Error error)
{
    setMessage("Cannot connect to remote device.");
    qWarning() << "Controller Error:" << error;
}
//! [Error handling]


//! [Find HRM characteristic]
void HeartRate::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::ServiceDiscovered:
    {
        const QLowEnergyCharacteristic hrChar = m_service->characteristic(
                    QBluetoothUuid(QBluetoothUuid::HeartRateMeasurement));
        if (!hrChar.isValid()) {
            setMessage("HR Data not found.");
            break;
        }

        const QLowEnergyDescriptor m_notificationDesc = hrChar.descriptor(
                    QBluetoothUuid::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid()) {
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));
            setMessage("Measuring");
            m_start = QDateTime::currentDateTime();
        }

        break;
    }
    default:
        //nothing for now
        break;
    }
}
//! [Find HRM characteristic]

void HeartRate::serviceError(QLowEnergyService::ServiceError e)
{
    switch (e) {
    case QLowEnergyService::DescriptorWriteError:
        setMessage("Cannot obtain HR notifications");
        break;
    default:
        qWarning() << "HR service error:" << e;
    }
}

//! [Reading value 1]
void HeartRate::updateHeartRateValue(const QLowEnergyCharacteristic &c,
                                     const QByteArray &value)
{
    // ignore any other characteristic change -> shouldn't really happen though
    if (c.uuid() != QBluetoothUuid(QBluetoothUuid::HeartRateMeasurement))
        return;


    const char *data = value.constData();
    quint8 flags = data[0];

    //Heart Rate
    if (flags & 0x1) { // HR 16 bit? otherwise 8 bit
        quint16 *heartRate = (quint16 *) &data[1];
        //qDebug() << "16 bit HR value:" << *heartRate;
        m_measurements.append(*heartRate);
    } else {
        quint8 *heartRate = (quint8 *) &data[1];
        m_measurements.append(*heartRate);
        //qDebug() << "8 bit HR value:" << *heartRate;
    }

    //Energy Expended
    if (flags & 0x8) {
        int index = (flags & 0x1) ? 5 : 3;
        quint16 *energy = (quint16 *) &data[index];
        qDebug() << "Used Energy:" << *energy;
    }
    //! [Reading value 1]

    Q_EMIT hrChanged();
//! [Reading value 2]
}
//! [Reading value 2]

void HeartRate::confirmedDescriptorWrite(const QLowEnergyDescriptor &d,
                                         const QByteArray &value)
{
    if (d.isValid() && d == m_notificationDesc && value == QByteArray("0000")) {
        //disabled notifications -> assume disconnect intent
        m_control->disconnectFromDevice();
        delete m_service;
        m_service = 0;
    }
}

int HeartRate::hR() const
{
    if (m_measurements.isEmpty())
        return 0;
    return m_measurements.last();
}

void HeartRate::obtainResults()
{
    Q_EMIT timeChanged();
    Q_EMIT averageChanged();
    Q_EMIT caloriesChanged();
}

int HeartRate::time()
{
    return m_start.secsTo(m_stop);
}

int HeartRate::maxHR() const
{
    return m_max;
}

int HeartRate::minHR() const
{
    return m_min;
}

float HeartRate::average()
{
    if (m_measurements.size() == 0) {
        return 0;
    } else {
        m_max = 0;
        m_min = 1000;
        int sum = 0;
        for (int i = 0; i < m_measurements.size(); i++) {
            sum += (int) m_measurements.value(i);
            if (((int)m_measurements.value(i)) > m_max)
                m_max = (int)m_measurements.value(i);
            if (((int)m_measurements.value(i)) < m_min)
                m_min = (int)m_measurements.value(i);
        }
        return sum/m_measurements.size();
    }
}

int HeartRate::measurements(int index) const
{
    if (index > m_measurements.size())
        return 0;
    else
        return (int)m_measurements.value(index);
}

int HeartRate::measurementsSize() const
{
    return m_measurements.size();
}

QString HeartRate::deviceAddress() const
{
    return m_currentDevice.getAddress();
}

float HeartRate::caloriesCalculation()
{
    calories = ((-55.0969 + (0.6309 * average()) + (0.1988 * 94) + (0.2017 * 24)) / 4.184) * 60 * time()/3600 ;
    return calories;
}

int HeartRate::numDevices() const
{
    return m_devices.size();
}

void HeartRate::startDemo()
{
    m_start = QDateTime::currentDateTime();
    if (!timer) {
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(receiveDemo()));
    }
    timer->start(1000);
    setMessage("This is Demo mode");
}

void HeartRate::receiveDemo()
{
    m_measurements.append(randomPulse());
    Q_EMIT hrChanged();
}

int HeartRate::randomPulse() const
{
    // random number between 50 and 70
    return qrand() % (70 - 50) + 50;
}
