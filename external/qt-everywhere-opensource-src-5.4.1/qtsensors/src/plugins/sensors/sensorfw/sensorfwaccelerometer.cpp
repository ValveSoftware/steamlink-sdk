/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "sensorfwaccelerometer.h"

char const * const sensorfwaccelerometer::id("sensorfw.accelerometer");

sensorfwaccelerometer::sensorfwaccelerometer(QSensor *sensor)
    : SensorfwSensorBase(sensor),
      m_initDone(false)
{
    init();
    setDescription(QLatin1String("x, y, and z axes accelerations in m/s^2"));
    setRanges(GRAVITY_EARTH_THOUSANDTH);
    setReading<QAccelerometerReading>(&m_reading);
    sensor->setDataRate(50);//set a default rate
}

void sensorfwaccelerometer::slotDataAvailable(const XYZ& data)
{
    // Convert from milli-Gs to meters per second per second
    // Using 1 G = 9.80665 m/s^2
    m_reading.setX(-data.x() * GRAVITY_EARTH_THOUSANDTH);
    m_reading.setY(-data.y() * GRAVITY_EARTH_THOUSANDTH);
    m_reading.setZ(-data.z() * GRAVITY_EARTH_THOUSANDTH);
    m_reading.setTimestamp(data.XYZData().timestamp_);
    newReadingAvailable();
}

void sensorfwaccelerometer::slotFrameAvailable(const QVector<XYZ>&  frame)
{
    for (int i=0, l=frame.size(); i<l; i++) {
        slotDataAvailable(frame.at(i));
    }
}

bool sensorfwaccelerometer::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    if (m_bufferSize==1)
        return QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(XYZ)), this, SLOT(slotDataAvailable(XYZ)));
    return QObject::connect(m_sensorInterface, SIGNAL(frameAvailable(QVector<XYZ>)),this, SLOT(slotFrameAvailable(QVector<XYZ>)));
}


QString sensorfwaccelerometer::sensorName() const
{
    return "accelerometersensor";
}


qreal sensorfwaccelerometer::correctionFactor() const
{
    return GRAVITY_EARTH_THOUSANDTH;
}

void sensorfwaccelerometer::init()
{
    m_initDone = false;
    initSensor<AccelerometerSensorChannelInterface>(m_initDone);
}

void sensorfwaccelerometer::start()
{
    if (reinitIsNeeded)
        init();
    SensorfwSensorBase::start();
}
