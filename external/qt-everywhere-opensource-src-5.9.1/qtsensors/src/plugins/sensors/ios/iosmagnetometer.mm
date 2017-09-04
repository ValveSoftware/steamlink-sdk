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

#include "iosmotionmanager.h"
#include "iosmagnetometer.h"

#import <CoreMotion/CoreMotion.h>

QT_BEGIN_NAMESPACE

char const * const IOSMagnetometer::id("ios.magnetometer");

int IOSMagnetometer::s_magnetometerStartCount = 0;
int IOSMagnetometer::s_deviceMotionStartCount = 0;

IOSMagnetometer::IOSMagnetometer(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
    , m_returnGeoValues(false)
{
    setReading<QMagnetometerReading>(&m_reading);
    // Technical information about data rate is not found, but
    // seems to be ~70Hz after testing on iPad4:
    addDataRate(1, 70);
    // Output range is +/- 2 gauss (0.0002 tesla) and can sense magnetic fields less than
    // 100 microgauss (1e-08 tesla) Ref: "iOS Sensor Programming", Alasdair, 2012.
    addOutputRange(-0.0002, 0.0002, 1e-08);
}

void IOSMagnetometer::start()
{
    if (m_timer != 0)
        return;

    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    m_returnGeoValues = static_cast<QMagnetometer *>(sensor())->returnGeoValues();

    if (m_returnGeoValues) {
        if (++s_deviceMotionStartCount == 1)
            [m_motionManager startDeviceMotionUpdates];
    } else {
        if (++s_magnetometerStartCount == 1)
            [m_motionManager startMagnetometerUpdates];
    }
}

void IOSMagnetometer::stop()
{
    if (m_timer == 0)
        return;

    killTimer(m_timer);
    m_timer = 0;

    if (m_returnGeoValues) {
        if (--s_deviceMotionStartCount == 0)
            [m_motionManager stopDeviceMotionUpdates];
    } else {
        if (--s_magnetometerStartCount == 0)
            [m_motionManager stopMagnetometerUpdates];
    }
}

void IOSMagnetometer::timerEvent(QTimerEvent *)
{
    CMMagneticField field;

    if (m_returnGeoValues) {
        CMDeviceMotion *deviceMotion = m_motionManager.deviceMotion;
        CMCalibratedMagneticField calibratedField = deviceMotion.magneticField;
        field = calibratedField.field;
        // skip update if NaN
        if (field.x != field.x || field.y != field.y || field.z != field.z)
            return;
        m_reading.setTimestamp(quint64(deviceMotion.timestamp * 1e6));

        switch (calibratedField.accuracy) {
        case CMMagneticFieldCalibrationAccuracyUncalibrated:
            m_reading.setCalibrationLevel(0.0);
            break;
        case CMMagneticFieldCalibrationAccuracyLow:
            m_reading.setCalibrationLevel(0.3);
            break;
        case CMMagneticFieldCalibrationAccuracyMedium:
            m_reading.setCalibrationLevel(0.6);
            break;
        case CMMagneticFieldCalibrationAccuracyHigh:
            m_reading.setCalibrationLevel(1.0);
            break;
        }
    } else {
        CMMagnetometerData *data = m_motionManager.magnetometerData;
        field = data.magneticField;
        // skip update if NaN
        if (field.x != field.x || field.y != field.y || field.z != field.z)
            return;
        m_reading.setTimestamp(quint64(data.timestamp * 1e6));
        m_reading.setCalibrationLevel(1.0);
    }

    // Convert NSTimeInterval to microseconds and microtesla to tesla:
    m_reading.setX(qreal(field.x) / 1e6);
    m_reading.setY(qreal(field.y) / 1e6);
    m_reading.setZ(qreal(field.z) / 1e6);
    newReadingAvailable();
}

QT_END_NAMESPACE
