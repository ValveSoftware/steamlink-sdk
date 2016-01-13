/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "iosmotionmanager.h"
#include "iosgyroscope.h"

char const * const IOSGyroscope::id("ios.gyroscope");

QT_BEGIN_NAMESPACE

IOSGyroscope::IOSGyroscope(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
{
    setReading<QGyroscopeReading>(&m_reading);
    addDataRate(1, 100); // 100Hz is max it seems
    addOutputRange(-360, 360, 0.01);
}

void IOSGyroscope::start()
{
    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    [m_motionManager startGyroUpdates];
}

void IOSGyroscope::stop()
{
    [m_motionManager stopGyroUpdates];
    killTimer(m_timer);
    m_timer = 0;
}

void IOSGyroscope::timerEvent(QTimerEvent *)
{
    // Convert NSTimeInterval to microseconds and radians to degrees:
    CMGyroData *data = m_motionManager.gyroData;
    CMRotationRate rate = data.rotationRate;
    // skip update if NaN
    if (rate.x != rate.x || rate.y != rate.y || rate.z != rate.z)
        return;
    m_reading.setTimestamp(quint64(data.timestamp * 1e6));
    m_reading.setX((qreal(rate.x) / M_PI) * 180);
    m_reading.setY((qreal(rate.y) / M_PI) * 180);
    m_reading.setZ((qreal(rate.z) / M_PI) * 180);
    newReadingAvailable();
}

QT_END_NAMESPACE
