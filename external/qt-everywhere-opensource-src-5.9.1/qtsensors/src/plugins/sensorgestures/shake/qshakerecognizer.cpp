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

#include <QDebug>
#include <QTimer>

#include "qshakerecognizer.h"

QShakeSensorGestureRecognizer::QShakeSensorGestureRecognizer(QObject *parent)
    : QSensorGestureRecognizer(parent)
    , timerTimeout(450)
    , active(0)
    , shaking(0)
    , shakeCount(0)
{
}

QShakeSensorGestureRecognizer::~QShakeSensorGestureRecognizer()
{
}

void QShakeSensorGestureRecognizer::create()
{
    accel = new QAccelerometer(this);
    accel->connectToBackend();
    accel->setDataRate(50);

    qoutputrangelist outputranges = accel->outputRanges();

    if (outputranges.count() > 0)
        accelRange = (int)(outputranges.at(0).maximum *2) / 9.8; //approx range in g's
    else
        accelRange = 4; //this should never happen

    connect(accel,SIGNAL(readingChanged()),this,SLOT(accelChanged()));
    timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(timeout()));
    timer->setSingleShot(true);
    timer->setInterval(timerTimeout);
}

bool QShakeSensorGestureRecognizer::start()
{
    active = accel->start();
    return active;
}

bool QShakeSensorGestureRecognizer::stop()
{
    accel->stop();
    active = accel->isActive();
    return !active;
}

bool QShakeSensorGestureRecognizer::isActive()
{
    return active;
}

QString QShakeSensorGestureRecognizer::id() const
{
    return QString("QtSensors.shake");
}

#define NUMBER_SHAKES 3
#define THRESHOLD 25

void QShakeSensorGestureRecognizer::accelChanged()
{
    qreal x = accel->reading()->x();
    qreal y = accel->reading()->y();
    qreal z = accel->reading()->z();

    currentData.x = x;
    currentData.y = y;
    currentData.z = z;

    if (qAbs(prevData.x - currentData.x)  < 1
            && qAbs(prevData.y - currentData.y)  < 1
            && qAbs(prevData.z - currentData.z)  < 1) {
        prevData.x = currentData.x;
        prevData.y = currentData.y;
        prevData.z = currentData.z;
        return;
    }

    bool wasShake = checkForShake(prevData, currentData, THRESHOLD);
    if (!shaking && wasShake &&
        shakeCount >= NUMBER_SHAKES) {
        shaking = true;
        shakeCount = 0;
        Q_EMIT shake();
        Q_EMIT detected("shake");

    } else if (wasShake) {

        shakeCount++;
        if (shakeCount > NUMBER_SHAKES) {
            timer->start();
        }
    }

    prevData.x = currentData.x;
    prevData.y = currentData.y;
    prevData.z = currentData.z;
}

void QShakeSensorGestureRecognizer::timeout()
{
    shakeCount = 0;
    shaking = false;
}

bool QShakeSensorGestureRecognizer::checkForShake(AccelData prevSensorData, AccelData currentSensorData, qreal threshold)
{
    double deltaX = qAbs(prevSensorData.x - currentSensorData.x);
    double deltaY = qAbs(prevSensorData.y - currentSensorData.y);
    double deltaZ = qAbs(prevSensorData.z - currentSensorData.z);

    return (deltaX > threshold
            || deltaY > threshold
            || deltaZ > threshold);
}

