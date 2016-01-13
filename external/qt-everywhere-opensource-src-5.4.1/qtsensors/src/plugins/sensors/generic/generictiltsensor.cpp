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

#include "generictiltsensor.h"
#include <QDebug>
#define _USE_MATH_DEFINES
#include <qmath.h>

char const * const GenericTiltSensor::id("generic.tilt");

GenericTiltSensor::GenericTiltSensor(QSensor *sensor)
    : QSensorBackend(sensor)
    , radAccuracy(M_PI / 180)
    , pitch(0)
    , roll(0)
    , calibratedPitch(0)
    , calibratedRoll(0)
    , xRotation(0)
    , yRotation(0)
{
    accelerometer = new QAccelerometer(this);
    accelerometer->addFilter(this);
    accelerometer->connectToBackend();

    setReading<QTiltReading>(&m_reading);
    setDataRates(accelerometer);
}

void GenericTiltSensor::start()
{
    accelerometer->setDataRate(sensor()->dataRate());
    accelerometer->setAlwaysOn(sensor()->isAlwaysOn());
    accelerometer->start();
    if (!accelerometer->isActive())
        sensorStopped();
    if (accelerometer->isBusy())
        sensorBusy();
}

void GenericTiltSensor::stop()
{
    accelerometer->stop();
}

/*
  Angle between Ground and X
                |            Ax           |
  pitch = arctan| ----------------------- |
                |  sqrt(Ay * Ay + Az * Az)|
*/
static inline qreal calcPitch(double Ax, double Ay, double Az)
{
    return -qAtan2(Ax, sqrt(Ay * Ay + Az * Az));
}

/*
  Angle between Ground and Y
                |            Ay           |
   roll = arctan| ----------------------- |
                |  sqrt(Ax * Ax + Az * Az)|
*/
static inline qreal calcRoll(double Ax, double Ay, double Az)
{
    return qAtan2(Ay, (sqrt(Ax * Ax + Az * Az)));
}

void GenericTiltSensor::calibrate()
{
    calibratedPitch = pitch;
    calibratedRoll = roll;
}

static qreal rad2deg(qreal rad)
{
    return rad / (2 * M_PI) * 360;
}

bool GenericTiltSensor::filter(QAccelerometerReading *reading)
{
    /*
      z  y
      | /
      |/___ x
    */

    qreal ax = reading->x();
    qreal ay = reading->y();
    qreal az = reading->z();
#ifdef LOGCALIBRATION
    qDebug() << "------------ new value -----------";
    qDebug() << "old _pitch: " << pitch;
    qDebug() << "old _roll: " << roll;
    qDebug() << "_calibratedPitch: " << calibratedPitch;
    qDebug() << "_calibratedRoll: " << calibratedRoll;
#endif
    pitch = calcPitch(ax, ay, az);
    roll  = calcRoll (ax, ay, az);
#ifdef LOGCALIBRATION
    qDebug() << "_pitch: " << pitch;
    qDebug() << "_roll: " << roll;
#endif
    qreal xrot = roll - calibratedRoll;
    qreal yrot = pitch - calibratedPitch;
    //get angle between 0 and 180 or 0 -180
    qreal aG = 1 * sin(xrot);
    qreal aK = 1 * cos(xrot);
    xrot = qAtan2(aG, aK);
    if (xrot > M_PI_2)
        xrot = M_PI - xrot;
    else if (xrot < -M_PI_2)
        xrot = -(M_PI + xrot);
    aG = 1 * sin(yrot);
    aK = 1 * cos(yrot);
    yrot = qAtan2(aG, aK);
    if (yrot > M_PI_2)
        yrot = M_PI - yrot;
    else if (yrot < -M_PI_2)
        yrot = -(M_PI + yrot);


#ifdef LOGCALIBRATION
    qDebug() << "new xrot: " << xrot;
    qDebug() << "new yrot: " << yrot;
    qDebug() << "----------------------------------";
#endif
    qreal dxrot = rad2deg(xrot) - xRotation;
    qreal dyrot = rad2deg(yrot) - yRotation;
    if (dxrot < 0) dxrot = -dxrot;
    if (dyrot < 0) dyrot = -dyrot;

    bool setNewReading = false;
    if (dxrot >= rad2deg(radAccuracy) || !sensor()->skipDuplicates()) {
        xRotation = rad2deg(xrot);
        setNewReading = true;
    }
    if (dyrot >= rad2deg(radAccuracy) || !sensor()->skipDuplicates()) {
        yRotation = rad2deg(yrot);
        setNewReading = true;
    }

    if (setNewReading || m_reading.timestamp() == 0) {
        m_reading.setTimestamp(reading->timestamp());
        m_reading.setXRotation(xRotation);
        m_reading.setYRotation(yRotation);
        newReadingAvailable();
    }

    return false;
}

bool GenericTiltSensor::isFeatureSupported(QSensor::Feature feature) const
{
    return (feature == QSensor::SkipDuplicates);
}
