/****************************************************************************
**
** Copyright (C) 2016 Lorn Potter
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
#include <qmath.h>

#include "qfreefallsensorgesturerecognizer.h"

QT_BEGIN_NAMESPACE

QFreefallSensorGestureRecognizer::QFreefallSensorGestureRecognizer(QObject *parent)
    : QSensorGestureRecognizer(parent)
    , active(0)
    , detecting(0)
{
}

QFreefallSensorGestureRecognizer::~QFreefallSensorGestureRecognizer()
{
}

void QFreefallSensorGestureRecognizer::create()
{
}

QString QFreefallSensorGestureRecognizer::id() const
{
    return QString("QtSensors.freefall");
}

bool QFreefallSensorGestureRecognizer::start()
{
    if (QtSensorGestureSensorHandler::instance()->startSensor(QtSensorGestureSensorHandler::Accel)) {
        active = true;
        connect(QtSensorGestureSensorHandler::instance(),SIGNAL(accelReadingChanged(QAccelerometerReading*)),
                this,SLOT(accelChanged(QAccelerometerReading*)));
    } else {
        active = false;
    }
    return active;

}

bool QFreefallSensorGestureRecognizer::stop()
{
    QtSensorGestureSensorHandler::instance()->stopSensor(QtSensorGestureSensorHandler::Accel);
    disconnect(QtSensorGestureSensorHandler::instance(),SIGNAL(accelReadingChanged(QAccelerometerReading*)),
            this,SLOT(accelChanged(QAccelerometerReading*)));
    active = false;

    return active;
}

bool QFreefallSensorGestureRecognizer::isActive()
{
    return active;
}

#define FREEFALL_THRESHOLD 1.0
#define LANDED_THRESHOLD 20.0
#define FREEFALL_MAX 4

void QFreefallSensorGestureRecognizer::accelChanged(QAccelerometerReading *reading)
{
    const qreal x = reading->x();
    const qreal y = reading->y();
    const qreal z = reading->z();
    qreal sum = qSqrt(x * x + y * y + z * z);

    if (qAbs(sum) < FREEFALL_THRESHOLD) {
        detecting = true;
        freefallList.append(sum);
    } else {
        if (detecting && qAbs(sum) > LANDED_THRESHOLD) {
            Q_EMIT landed();
            Q_EMIT detected("landed");
            freefallList.clear();
        }
    }

    if (freefallList.count() > FREEFALL_MAX) {
        Q_EMIT freefall();
        Q_EMIT detected("freefall");
    }
}


QT_END_NAMESPACE

