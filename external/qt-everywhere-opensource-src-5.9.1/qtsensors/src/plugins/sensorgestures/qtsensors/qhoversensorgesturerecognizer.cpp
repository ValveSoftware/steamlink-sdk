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


#include "qhoversensorgesturerecognizer.h"
#include <math.h>

#define TIMER2_TIMEOUT 5000

QT_BEGIN_NAMESPACE

QHoverSensorGestureRecognizer::QHoverSensorGestureRecognizer(QObject *parent) :
    QSensorGestureRecognizer(parent),
    orientationReading(0),reflectance(0),
    hoverOk(0), detecting(0), active(0), initialReflectance(0), useHack(0),
    lastTimestamp(0), timer2Active(0), lapsedTime2(0)
{
}

QHoverSensorGestureRecognizer::~QHoverSensorGestureRecognizer()
{
}

void QHoverSensorGestureRecognizer::create()
{

}

QString QHoverSensorGestureRecognizer::id() const
{
    return QString("QtSensors.hover");
}

bool QHoverSensorGestureRecognizer::start()
{
    if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::IrProximity)) {
        if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Orientation)) {
            active = true;
            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(irProximityReadingChanged(QIRProximityReading*)),
                    this,SLOT(irProximityReadingChanged(QIRProximityReading*)));
            connect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
                    this,SLOT(orientationReadingChanged(QOrientationReading*)));
        } else {
            QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::IrProximity);
            active = false;
        }
    } else {
        active = false;
    }

    detecting = false;
    detectedHigh = 0;
    initialReflectance = 0;
    useHack = false;
    timer2Active = false;
    lapsedTime2 = 0;
    return active;
}

bool QHoverSensorGestureRecognizer::stop()
{
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::IrProximity);
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Orientation);
    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(irProximityReadingChanged(QIRProximityReading*)),
            this,SLOT(irProximityReadingChanged(QIRProximityReading*)));
    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(orientationReadingChanged(QOrientationReading*)),
            this,SLOT(orientationReadingChanged(QOrientationReading*)));
    active = false;
    timer2Active = false;
    initialReflectance = 0;
    return active;
}

bool QHoverSensorGestureRecognizer::isActive()
{
    return active;
}


void QHoverSensorGestureRecognizer::orientationReadingChanged(QOrientationReading *reading)
{
    orientationReading = reading;
}

void QHoverSensorGestureRecognizer::irProximityReadingChanged(QIRProximityReading *reading)
{
    reflectance = reading->reflectance();
    if (reflectance == 0)
        return;

    if (initialReflectance == 0) {
        initialReflectance = reflectance;
    }

    if (initialReflectance > .2) {
        useHack = true;
        initialReflectance -= .1;
    }
    if (useHack)
        reflectance -= .1;

    if (detecting && !hoverOk) {
        detectedHigh = qMax(detectedHigh, reflectance);
    }

    if (reflectance > 0.4) {
        // if close stop detecting
        hoverOk = false;
        detecting = false;
        detectedHigh = 0;
    }

    qreal detectedPercent =  100 - (detectedHigh / reflectance * 100);

    qint16 percentCheck;
    if (useHack)
        percentCheck = -60;
    else
        percentCheck = -101;

    quint64 timestamp = reading->timestamp();

    if (!detecting
            && checkForHovering()) {
        detecting = true;
        detecting = true;
        timer2Active = true;
        detectedHigh = reflectance;
    } else if (detecting
                && detectedPercent < percentCheck
               && !checkForHovering()) {
        // went light again after 1 seconds
        Q_EMIT hover();
        Q_EMIT detected("hover");
        hoverOk = false;
        detecting = false;
        detectedHigh = 0;
        timer2Active = false;;
    }
    if (detecting && reflectance <  0.2) {
        timeout();
    }
    if (timer2Active && lastTimestamp > 0)
        lapsedTime2 += (timestamp - lastTimestamp )/1000;

    if (timer2Active && lapsedTime2 >= TIMER2_TIMEOUT) {
        timeout2();
    }

    lastTimestamp = reading->timestamp();
}

bool QHoverSensorGestureRecognizer::checkForHovering()
{
    if (orientationReading == 0) {
        return false;
    }
    if (orientationReading->orientation() != QOrientationReading::FaceUp)
        return false;
    if ( (reflectance > 0.2 && reflectance < 0.4)
         && (initialReflectance - reflectance) < -0.1)
        return true;

    return false;
}


void QHoverSensorGestureRecognizer::timeout()
{
    if (checkForHovering()) {
        hoverOk = true;
        timer2Active = true;
    } else {
        detecting = false;
        detectedHigh = 0;
    }
}

void QHoverSensorGestureRecognizer::timeout2()
{
    detecting = false;
    hoverOk = false;
    detectedHigh = 0;
}

QT_END_NAMESPACE
