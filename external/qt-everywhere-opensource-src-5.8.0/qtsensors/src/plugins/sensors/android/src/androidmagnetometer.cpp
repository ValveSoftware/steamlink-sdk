/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bogdan@kde.org>
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

#include "androidmagnetometer.h"

enum AndroidSensorStatus
{
    SENSOR_STATUS_UNRELIABLE = 0,
    SENSOR_STATUS_ACCURACY_LOW = 1,
    SENSOR_STATUS_ACCURACY_MEDIUM = 2,
    SENSOR_STATUS_ACCURACY_HIGH = 3,
};

AndroidMagnetometer::AndroidMagnetometer(AndroidSensors::AndroidSensorType type, QSensor *sensor)
    :AndroidCommonSensor<QMagnetometerReading>(type, sensor)
{}

void AndroidMagnetometer::onAccuracyChanged(jint accuracy)
{
    // Expected range is [0, 3]
    if (accuracy < SENSOR_STATUS_UNRELIABLE || accuracy > SENSOR_STATUS_ACCURACY_HIGH) {
        qWarning("Unable to get sensor accuracy. Unexpected value: %d", accuracy);
        return;
    }

    m_reader.setCalibrationLevel(accuracy / qreal(3.0));
}

void AndroidMagnetometer::onSensorChanged(jlong timestamp, const jfloat *values, uint size)
{
    if (size<3)
        return;
    m_reader.setTimestamp(timestamp/1000);
    // check https://developer.android.com/reference/android/hardware/SensorEvent.html#values
    m_reader.setX(values[0]/1e6);
    m_reader.setY(values[1]/1e6);
    m_reader.setZ(values[2]/1e6);
    newReadingAvailable();
}
