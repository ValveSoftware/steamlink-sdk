/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#ifndef HEARTRATE_H
#define HEARTRATE_H

#include "deviceinfo.h"

#include <QString>
#include <QDebug>
#include <QDateTime>
#include <QVector>
#include <QTimer>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>


QT_USE_NAMESPACE
class HeartRate: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(int hr READ hR NOTIFY hrChanged)
    Q_PROPERTY(int maxHR READ maxHR NOTIFY averageChanged)
    Q_PROPERTY(int minHR READ minHR NOTIFY averageChanged)
    Q_PROPERTY(float average READ average NOTIFY averageChanged)
    Q_PROPERTY(int time READ time NOTIFY timeChanged)
    Q_PROPERTY(float calories READ caloriesCalculation NOTIFY caloriesChanged)

public:
    HeartRate();
    ~HeartRate();
    void setMessage(QString message);
    QString message() const;
    QVariant name();
    int hR() const;
    int time();
    float average();
    int maxHR() const;
    int minHR() const;
    float caloriesCalculation();

public slots:
    void deviceSearch();
    void connectToService(const QString &address);
    void disconnectService();
    void startDemo();

    void obtainResults();
    int measurements(int index) const;
    int measurementsSize() const;
    QString deviceAddress() const;
    int numDevices() const;

private slots:
    //QBluetothDeviceDiscoveryAgent
    void addDevice(const QBluetoothDeviceInfo&);
    void scanFinished();
    void deviceScanError(QBluetoothDeviceDiscoveryAgent::Error);

    //QLowEnergyController
    void serviceDiscovered(const QBluetoothUuid &);
    void serviceScanDone();
    void controllerError(QLowEnergyController::Error);
    void deviceConnected();
    void deviceDisconnected();


    //QLowEnergyService
    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void updateHeartRateValue(const QLowEnergyCharacteristic &c,
                              const QByteArray &value);
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &d,
                              const QByteArray &value);
    void serviceError(QLowEnergyService::ServiceError e);

    //DemoMode
    void receiveDemo();

Q_SIGNALS:
    void messageChanged();
    void nameChanged();
    void hrChanged();
    void averageChanged();
    void timeChanged();
    void caloriesChanged();

private:
    int randomPulse() const;

    DeviceInfo m_currentDevice;
    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent;
    QLowEnergyDescriptor m_notificationDesc;
    QList<QObject*> m_devices;
    QString m_info;
    bool foundHeartRateService;
    QVector<quint16> m_measurements;
    QDateTime m_start;
    QDateTime m_stop;
    int m_max;
    int m_min;
    float calories;
    QLowEnergyController *m_control;
    QTimer *timer; // for demo application
    QLowEnergyService *m_service;
};

#endif // HEARTRATE_H
