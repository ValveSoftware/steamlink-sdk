/****************************************************************************
**
** Copyright (C) 2013 BogDan Vatra <bogdan@kde.org>
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
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

#include "androidaccelerometer.h"

AndroidAccelerometer::AndroidAccelerometer(AndroidSensors::AndroidSensorType type, QSensor *sensor)
    : AndroidCommonSensor<QAccelerometerReading>(type, sensor)
{
    QAccelerometer * const accelerometer = qobject_cast<QAccelerometer *>(sensor);
    if (accelerometer) {
        connect(accelerometer, SIGNAL(accelerationModeChanged(AccelerationMode)),
                this, SLOT(applyAccelerationMode()));
    }
}

void AndroidAccelerometer::onSensorChanged(jlong timestamp, const jfloat *values, uint size)
{
    if (size < 3)
        return;
    m_reader.setTimestamp(timestamp/1000);
    // check https://developer.android.com/reference/android/hardware/SensorEvent.html#values
    m_reader.setX(values[0]);
    m_reader.setY(values[1]);
    m_reader.setZ(values[2]);
    newReadingAvailable();
}

void AndroidAccelerometer::onAccuracyChanged(jint accuracy)
{
    Q_UNUSED(accuracy)
}

void AndroidAccelerometer::applyAccelerationMode()
{
    const QAccelerometer * const accelerometer = qobject_cast<QAccelerometer *>(sensor());
    if (accelerometer) {
        stop(); //Stop previous sensor and start new one
        m_type = modeToSensor(accelerometer->accelerationMode());
        start();
    }
}
AndroidSensors::AndroidSensorType AndroidAccelerometer::modeToSensor(QAccelerometer::AccelerationMode mode)
{
    AndroidSensors::AndroidSensorType type;

    switch (mode) {
    case QAccelerometer::Gravity:
        type = AndroidSensors::TYPE_GRAVITY;
        break;
    case QAccelerometer::User:
        type = AndroidSensors::TYPE_LINEAR_ACCELERATION;
        break;
    case QAccelerometer::Combined:
    default:
        type = AndroidSensors::TYPE_ACCELEROMETER;
        break;
    }

    if (type != AndroidSensors::TYPE_ACCELEROMETER && !AndroidSensors::availableSensors().contains(type))
        type = AndroidSensors::TYPE_ACCELEROMETER;

    return type;
}
