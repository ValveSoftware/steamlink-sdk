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
    m_accelerometerListener->start(sensor()->dataRate());
    if (!m_magnetometerListener)
        m_magnetometerListener = new AndroidMagnetometerListener(this);
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
    qreal azimuth = AndroidSensors::getCompassAzimuth(m_accelerometerListener->reading, m_magnetometerListener->reading);

    azimuth = azimuth * 180.0 / M_PI;
    m_reading.setAzimuth(azimuth);
    newReadingAvailable();
}
