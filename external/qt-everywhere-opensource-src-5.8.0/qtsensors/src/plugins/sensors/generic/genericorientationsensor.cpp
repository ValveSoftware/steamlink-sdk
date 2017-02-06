/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "genericorientationsensor.h"
#include <QDebug>

char const * const genericorientationsensor::id("generic.orientation");

genericorientationsensor::genericorientationsensor(QSensor *sensor)
    : QSensorBackend(sensor)
{
    accelerometer = new QAccelerometer(this);
    accelerometer->addFilter(this);
    accelerometer->connectToBackend();

    setReading<QOrientationReading>(&m_reading);
    setDataRates(accelerometer);
}

void genericorientationsensor::start()
{
    accelerometer->setDataRate(sensor()->dataRate());
    accelerometer->setAlwaysOn(sensor()->isAlwaysOn());
    accelerometer->start();
    if (!accelerometer->isActive())
        sensorStopped();
    if (accelerometer->isBusy())
        sensorBusy();
}

void genericorientationsensor::stop()
{
    accelerometer->stop();
}

bool genericorientationsensor::filter(QAccelerometerReading *reading)
{
    QOrientationReading::Orientation o = m_reading.orientation();

    if (reading->y() > 7.35)
        o = QOrientationReading::TopUp;
    else if (reading->y() < -7.35)
        o = QOrientationReading::TopDown;
    else if (reading->x() > 7.35)
        o = QOrientationReading::RightUp;
    else if (reading->x() < -7.35)
        o = QOrientationReading::LeftUp;
    else if (reading->z() > 7.35)
        o = QOrientationReading::FaceUp;
    else if (reading->z() < -7.35)
        o = QOrientationReading::FaceDown;

    if (o != m_reading.orientation() || m_reading.timestamp() == 0) {
        m_reading.setTimestamp(reading->timestamp());
        m_reading.setOrientation(o);
        newReadingAvailable();
    }

    return false;
}

