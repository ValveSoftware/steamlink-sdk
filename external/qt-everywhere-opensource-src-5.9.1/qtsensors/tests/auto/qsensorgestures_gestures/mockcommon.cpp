/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QTimer>
#include <QDebug>
#include <QCoreApplication>
#include "mockcommon.h"

Q_GLOBAL_STATIC(mockcommonPrivate, mockcommonPrv)

mockcommonPrivate *mockcommonPrivate::instance()
{
    return mockcommonPrv();
}

mockcommonPrivate::mockcommonPrivate()
    : QObject(),prevts(50), firstRun(0)
{
    readTimer = new QTimer(this);
    readTimer->setSingleShot(true);
    connect(readTimer,SIGNAL(timeout()),this,SLOT(timerout()));
}

void mockcommonPrivate::timerout()
{
    while (pFile.isOpen() && !pFile.atEnd()) {
        QString line = pFile.readLine();
        if (!line.isNull()) {
            if (parseData(line)) {
                readTimer->start();
                break;
            }
        }
    }
}

bool mockcommonPrivate::setFile(const QString &filename)
{
    if (filename.isEmpty())
        return false;

    if (pFile.isOpen()) {
        pFile.close();
    }
    oldAccelTs = 0;
    firstRun = true;
    pFile.setFileName(QCoreApplication::instance()->applicationDirPath() + "/" + filename);
    bool ok = pFile.open(QIODevice::ReadOnly);
    if (!ok) {
        pFile.setFileName(SRCDIR "/" + filename);
        ok = pFile.open(QIODevice::ReadOnly);
    }
    return ok;
}

bool mockcommonPrivate::parseData(const QString &line)
{
    bool ok = false;
    QString sensorToken = line.section(QLatin1String(":"),0, 0).trimmed();
    QString data = line.section(QLatin1String(":"),1, 1).trimmed();

    if (sensorToken == QLatin1String("accelerometer")) {
        if (!firstRun) {

            Q_EMIT accelData(data);
            if (prevts == 0 || prevts > 90000 )
                prevts = 20000; // use 20 Hz
            if (prevts > 90000 ) // original slam timestamps are wrong
                prevts = 100000; //use 100 Hz
            readTimer->setInterval((int)prevts/1000);
        } else {
            firstRun = false;
        }
        quint64 ts = data.section(QLatin1String(","), 0,0).toULongLong();
        ok = true;
        qreal difference;
        if (oldAccelTs == 0) {
            oldAccelTs = ts;
        }
        difference = ts - oldAccelTs;

        if (difference < 1
                || difference == ts) {
            int hertz = 50;
            readTimer->setInterval((1.0/hertz)*1000);
        } else {
            if (firstRun)
                readTimer->setInterval((int)difference/1000);
        }
        oldAccelTs = ts;
        prevts = difference;

    } else if (sensorToken == QLatin1String("irProximity")) {
        Q_EMIT irProxyData(data);
    } else if (sensorToken == QLatin1String("orientation")) {
        Q_EMIT orientData(data);
    } else if (sensorToken == QLatin1String("tap")) {
        Q_EMIT tapData(data); //just send this it takes only one to be detected
    } else if (sensorToken == QLatin1String("proximity")) {
        Q_EMIT proxyData(data);
    }
    return ok;
}


mockcommon::mockcommon(QSensor *sensor)
    : QSensorBackend(sensor), timer(0)
{
    mockcommonPrv()->readTimer->setInterval(0);
}

void mockcommon::start()
{
    if (!mockcommonPrv()->readTimer->isActive()) {
        mockcommonPrv()->readTimer->start();
    }
}

void mockcommon::stop()
{
    if (mockcommonPrv()->readTimer->isActive()) {
        mockcommonPrv()->readTimer->stop();
    }
}

char const * const mockaccelerometer::id("mock.accelerometer");

mockaccelerometer::mockaccelerometer(QSensor *sensor)
    : mockcommon(sensor)
{
    setReading<QAccelerometerReading>(&m_reading);
    addDataRate(50, 50); // 50

    connect(mockcommonPrv(),SIGNAL(accelData(QString)),this,SLOT(parseAccelData(QString)));
}

void mockaccelerometer::parseAccelData(const QString &data)
{
    quint64 ts = data.section(QLatin1String(","), 0,0).toULongLong();
    m_reading.setTimestamp(ts);
    m_reading.setX(data.section(QLatin1String(","), 1,1).toDouble());
    m_reading.setY(data.section(QLatin1String(","), 2,2).toDouble());
    m_reading.setZ(data.section(QLatin1String(","), 3,3).toDouble());
    newReadingAvailable();
}

char const * const mockorientationsensor::id("mock.orientation");

mockorientationsensor::mockorientationsensor(QSensor *sensor)
    : mockcommon(sensor)
{
    setReading<QOrientationReading>(&m_reading);
    addDataRate(50, 50); // 50Hz
    connect(mockcommonPrv(),SIGNAL(orientData(QString)),this,SLOT(parseOrientData(QString)));
}

void mockorientationsensor::parseOrientData(const QString &data)
{
    m_reading.setTimestamp(data.section(QLatin1String(","), 0,0).toULongLong());
    m_reading.setOrientation(static_cast<QOrientationReading::Orientation>(data.section(QLatin1String(","), 1,1).toInt()));

    newReadingAvailable();
}

char const * const mockirproximitysensor::id("mock.irproximity");

mockirproximitysensor::mockirproximitysensor(QSensor *sensor)
    : mockcommon(sensor)
{
    setReading<QIRProximityReading>(&m_reading);
    addDataRate(50, 50); // 50Hz
    connect(mockcommonPrv(),SIGNAL(irProxyData(QString)),this,SLOT(parseIrProxyData(QString)));
}

void mockirproximitysensor::parseIrProxyData(const QString &data)
{
    m_reading.setTimestamp(data.section(QLatin1String(","), 0,0).toULongLong());
    m_reading.setReflectance(data.section(QLatin1String(","), 1,1).toDouble());

    newReadingAvailable();
}

char const * const mocktapsensor::id("mock.tap");

mocktapsensor::mocktapsensor(QSensor *sensor)
    : mockcommon(sensor)
{
    setReading<QTapReading>(&m_reading);
    addDataRate(50, 50); // 50Hz
    connect(mockcommonPrv(),SIGNAL(tapData(QString)),this,SLOT(parseTapData(QString)));
}

void mocktapsensor::parseTapData(const QString &data)
{
    m_reading.setTimestamp(data.section(QLatin1String(","), 0,0).toULongLong());
    m_reading.setDoubleTap((data.section(QLatin1String(","), 1,1).toInt() == 1));

    newReadingAvailable();
}

char const * const mockproximitysensor::id("mock.proximity");

mockproximitysensor::mockproximitysensor(QSensor *sensor)
    : mockcommon(sensor)
{
    setReading<QProximityReading>(&m_reading);
    addDataRate(50, 50); // 50Hz
    connect(mockcommonPrv(),SIGNAL(proxyData(QString)),this,SLOT(parseProxyData(QString)));
}

void mockproximitysensor::parseProxyData(const QString &data)
{
    m_reading.setTimestamp(data.section(QLatin1String(","), 0,0).toULongLong());
    m_reading.setClose((data.section(QLatin1String(","), 1,1).toInt() == 1));

    newReadingAvailable();
}
