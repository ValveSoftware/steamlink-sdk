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

#include "androidcompass.h"

#include <QDebug>
#include <qmath.h>
#include "androidjnisensors.h"


class AndroidAccelerometerListener : public AndroidSensors::AndroidSensorsListenerInterface
{
public:

    AndroidAccelerometerListener(AndroidCompass *parent)
        : m_compass(parent)
    {
        memset(reading, 0, sizeof(reading));
    }

    void start(int dataRate)
    {
        AndroidSensors::registerListener(AndroidSensors::TYPE_ACCELEROMETER, this, dataRate);
    }

    void stop()
    {
        AndroidSensors::unregisterListener(AndroidSensors::TYPE_ACCELEROMETER, this);
    }

    void onAccuracyChanged(jint accuracy) Q_DECL_OVERRIDE
    {
        Q_UNUSED(accuracy);
    }

    void onSensorChanged(jlong /*timestamp*/, const jfloat *values, uint size) Q_DECL_OVERRIDE
    {
        if (size < 3)
            return;
        reading[0] = values[0];
        reading[1] = values[1];
        reading[2] = values[2];
        m_compass->testStuff();
    }

    jfloat reading[3];

private:
    AndroidCompass *m_compass;
};

class AndroidMagnetometerListener : public AndroidSensors::AndroidSensorsListenerInterface
{
public:
    AndroidMagnetometerListener(AndroidCompass *parent)
        :m_compass(parent)
    {
        memset(reading, 0, sizeof(reading));
    }

    void start(int dataRate)
    {
        AndroidSensors::registerListener(AndroidSensors::TYPE_MAGNETIC_FIELD, this, dataRate);
    }

    void stop()
    {
        AndroidSensors::unregisterListener(AndroidSensors::TYPE_MAGNETIC_FIELD, this);
    }

    void onAccuracyChanged(jint accuracy) Q_DECL_OVERRIDE
    {
        Q_UNUSED(accuracy);
    }

    void onSensorChanged(jlong /*timestamp*/, const jfloat *values, uint size) Q_DECL_OVERRIDE
    {
        if (size < 3)
            return;
        reading[0] = values[0];
        reading[1] = values[1];
        reading[2] = values[2];
        m_compass->testStuff();
    }

    jfloat reading[3];
private:
    AndroidCompass *m_compass;
};

char const * const AndroidCompass::id("android.synthetic.compass");

AndroidCompass::AndroidCompass(QSensor *sensor)
    : QSensorBackend(sensor), m_accelerometerListener(0), m_magnetometerListener(0), m_isStarted(false)
{
    setReading<QCompassReading>(&m_reading);
    m_isStarted = false;
}

AndroidCompass::~AndroidCompass()
{
    if (m_isStarted)
        stop();
    delete m_accelerometerListener;
    delete m_magnetometerListener;
}

void AndroidCompass::start()
{
    if (!m_accelerometerListener)
        m_accelerometerListener = new AndroidAccelerometerListener(this);
    if (!m_magnetometerListener)
        m_magnetometerListener = new AndroidMagnetometerListener(this);
    m_accelerometerListener->start(sensor()->dataRate());
    m_magnetometerListener->start(sensor()->dataRate());

    m_isStarted = true;
}

void AndroidCompass::stop()
{
    if (m_isStarted) {
        m_isStarted = false;
        m_accelerometerListener->stop();
        m_magnetometerListener->stop();
    }
}

void AndroidCompass::testStuff()
{
    if (!m_accelerometerListener || !m_magnetometerListener)
        return;
    qreal azimuth = AndroidSensors::getCompassAzimuth(m_accelerometerListener->reading, m_magnetometerListener->reading);

    azimuth = azimuth * 180.0 / M_PI;
    m_reading.setAzimuth(azimuth);
    newReadingAvailable();
}
