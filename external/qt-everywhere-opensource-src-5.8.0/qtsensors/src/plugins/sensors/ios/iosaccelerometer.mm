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

#include "iosaccelerometer.h"
#include "iosmotionmanager.h"

#import <CoreMotion/CoreMotion.h>

char const * const IOSAccelerometer::id("ios.accelerometer");

QT_BEGIN_NAMESPACE

int IOSAccelerometer::s_startCount = 0;

IOSAccelerometer::IOSAccelerometer(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
{
    setReading<QAccelerometerReading>(&m_reading);
    addDataRate(1, 100); // 100Hz
    addOutputRange(-22.418, 22.418, 0.17651); // 2G
}

void IOSAccelerometer::start()
{
    if (m_timer != 0)
        return;

    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    if (++s_startCount == 1)
        [m_motionManager startAccelerometerUpdates];
}

void IOSAccelerometer::stop()
{
    if (m_timer == 0)
        return;

    killTimer(m_timer);
    m_timer = 0;
    if (--s_startCount == 0)
        [m_motionManager stopAccelerometerUpdates];
}

void IOSAccelerometer::timerEvent(QTimerEvent *)
{
    // Convert from NSTimeInterval to microseconds and G to m/s2, and flip axes:
    CMAccelerometerData *data = m_motionManager.accelerometerData;
    CMAcceleration acc = data.acceleration;
    // skip update if NaN
    if (acc.x != acc.x || acc.y != acc.y || acc.z != acc.z)
        return;
    static const qreal G = 9.8066;
    m_reading.setTimestamp(quint64(data.timestamp * 1e6));
    m_reading.setX(qreal(acc.x) * G * -1);
    m_reading.setY(qreal(acc.y) * G * -1);
    m_reading.setZ(qreal(acc.z) * G * -1);
    newReadingAvailable();
}

QT_END_NAMESPACE
