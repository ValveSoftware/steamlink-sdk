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


#include "qwhipsensorgesturerecognizer.h"
#include "qtsensorgesturesensorhandler.h"

#include <QtCore/qmath.h>

#define TIMER_TIMEOUT 850

QT_BEGIN_NAMESPACE

QWhipSensorGestureRecognizer::QWhipSensorGestureRecognizer(QObject *parent)
    : QSensorGestureRecognizer(parent),
    orientationReading(0),
    accelRange(0),
    active(0),
    lastX(0),
    lastY(0),
    lastZ(0),
    detecting(0),
    whipOk(0)
  , lastTimestamp(0)
  , timerActive(0)
  , lapsedTime(0)
{
}

QWhipSensorGestureRecognizer::~QWhipSensorGestureRecognizer()
{
}

void QWhipSensorGestureRecognizer::create()
{
}

QString QWhipSensorGestureRecognizer::id() const
{
    return QString("QtSensors.whip");
}

bool QWhipSensorGestureRecognizer::start()
{
    if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Accel)) {
        if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Orientation)) {
            accelRange = QtSensorGestureSensorHandler::instance()->accelRange;
            active = true;
            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
                    this,SLOT(orientationReadingChanged(QOrientationReading*)));

            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(accelReadingChanged(QAccelerometerReading*)),
                    this,SLOT(accelChanged(QAccelerometerReading*)));
        } else {
            QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Accel);
            active = false;
        }
    } else {
        active = false;
    }
    lastTimestamp = 0;
    timerActive = false;
    lapsedTime = 0;
    return active;
}

bool QWhipSensorGestureRecognizer::stop()
{
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Accel);
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Orientation);
    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
            this,SLOT(orientationReadingChanged(QOrientationReading*)));

    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(accelReadingChanged(QAccelerometerReading*)),
            this,SLOT(accelChanged(QAccelerometerReading*)));
    active = false;
    return active;
}

bool QWhipSensorGestureRecognizer::isActive()
{
    return active;
}

void QWhipSensorGestureRecognizer::orientationReadingChanged(QOrientationReading *reading)
{
    orientationReading = reading;
}

#define WHIP_FACTOR -11.0
#define WHIP_WIGGLE_FACTOR 0.35

void QWhipSensorGestureRecognizer::accelChanged(QAccelerometerReading *reading)
{
    const qreal x = reading->x();
    const qreal y = reading->y();
    qreal z = reading->z();

    quint64 timestamp = reading->timestamp();

    if (zList.count() > 4)
        zList.removeLast();

    qreal averageZ = 0;
    Q_FOREACH (qreal az, zList) {
        averageZ += az;
    }
//    averageZ += z;
    averageZ /= zList.count();

    zList.insert(0,z);

    if (orientationReading == 0)
        return;
    //// very hacky
    if (orientationReading->orientation() == QOrientationReading::FaceUp) {
        z = z - 9.8;
    }

    const qreal diffX = lastX - x;
    const qreal diffY = lastY - y;

    if (detecting && whipMap.count() > 5 && whipMap.at(5) == true) {
        checkForWhip();
    }

    if (whipMap.count() > 5)
        whipMap.removeLast();

    if (negativeList.count() > 5)
        negativeList.removeLast();

    if (z < WHIP_FACTOR
            && qAbs(diffX) > -(accelRange * .1285)//-5.0115
            && qAbs(lastX) < 7
            && qAbs(x) < 7) {
        whipMap.insert(0,true);
        if (!detecting && !timerActive) {
            timerActive = true;
            detecting = true;
        }
    } else {
        whipMap.insert(0,false);
    }

    // check if shaking
    if ((((x < 0 && lastX > 0) || (x > 0 && lastX < 0))
         && qAbs(diffX) > (accelRange   * 0.7)) //27.3
            || (((y < 0 && lastY > 0) || (y > 0 && lastY < 0))
            && qAbs(diffY) > (accelRange * 0.7))) {
        negativeList.insert(0,true);
    } else {
        negativeList.insert(0,false);
    }

    lastX = x;
    lastY = y;
    lastZ = z;

    if (timerActive && lastTimestamp > 0)
        lapsedTime += (timestamp - lastTimestamp )/1000;

    if (timerActive && lapsedTime >= TIMER_TIMEOUT) {
        timeout();
    }
}

void QWhipSensorGestureRecognizer::timeout()
{
    detecting = false;
}


void QWhipSensorGestureRecognizer::checkForWhip()
{
    whipOk = false;

    int check = 0;
    Q_FOREACH (qreal az, zList) {
        if (az < -10)
            check++;
    }
    if (check >= 4)
        whipOk = true;
    else
        return;

    if (whipOk) {
        bool ok = true;
        for (int i = 0; i < negativeList.count() - 1; i++) {
            if (negativeList.at(i)) {
                ok = false;
            }
        }
        if (ok) {
            Q_EMIT whip();
            Q_EMIT detected("whip");
        }
        detecting = false;
        whipMap.clear();
        timerActive = false;
    }
}

QT_END_NAMESPACE
