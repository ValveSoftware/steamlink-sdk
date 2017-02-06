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

#include "genericalssensor.h"
#include <QDebug>

char const * const genericalssensor::id("generic.als");

genericalssensor::genericalssensor(QSensor *sensor)
    : QSensorBackend(sensor)
{
    lightSensor = new QLightSensor(this);
    lightSensor->addFilter(this);
    lightSensor->connectToBackend();

    setReading<QAmbientLightReading>(&m_reading);
    setDataRates(lightSensor);
}

void genericalssensor::start()
{
    lightSensor->setDataRate(sensor()->dataRate());
    lightSensor->setAlwaysOn(sensor()->isAlwaysOn());
    lightSensor->start();
    if (!lightSensor->isActive())
        sensorStopped();
    if (lightSensor->isBusy())
        sensorBusy();
}

void genericalssensor::stop()
{
    lightSensor->stop();
}

struct lux_limit {
    int min;
    int max;
};

// Defines the min and max lux values that a given level has.
// These are used to add histeresis to the sensor.
// If the previous level is below a level, the lux must be at or above the minimum.
// If the previous level is above a level, the lux muyt be at or below the maximum.
static lux_limit limits[] = {
    { 0,    0    }, // Undefined (not used)
    { 0,    5    }, // Dark
    { 10,   50   }, // Twilight
    { 100,  200  }, // Light
    { 500,  2000 }, // Bright
    { 5000, 0    }  // Sunny
};

#if 0
// Used for debugging
static QString light_level(int level)
{
    switch (level) {
    case 1:
        return QLatin1String("Dark");
    case 2:
        return QLatin1String("Twilight");
    case 3:
        return QLatin1String("Light");
    case 4:
        return QLatin1String("Bright");
    case 5:
        return QLatin1String("Sunny");
    default:
        return QLatin1String("Undefined");
    }
}
#endif

bool genericalssensor::filter(QLightReading *reading)
{
    // It's unweildly dealing with these constants so make some
    // local aliases that are shorter. This makes the code below
    // much easier to read.
    enum {
        Undefined = QAmbientLightReading::Undefined,
        Dark = QAmbientLightReading::Dark,
        Twilight = QAmbientLightReading::Twilight,
        Light = QAmbientLightReading::Light,
        Bright = QAmbientLightReading::Bright,
        Sunny = QAmbientLightReading::Sunny
    };

    int lightLevel = m_reading.lightLevel();
    qreal lux = reading->lux();

    // Check for change direction to allow for histeresis
    if (lightLevel < Sunny    && lux >= limits[Sunny   ].min) lightLevel = Sunny;
    else if (lightLevel < Bright   && lux >= limits[Bright  ].min) lightLevel = Bright;
    else if (lightLevel < Light    && lux >= limits[Light   ].min) lightLevel = Light;
    else if (lightLevel < Twilight && lux >= limits[Twilight].min) lightLevel = Twilight;
    else if (lightLevel < Dark     && lux >= limits[Dark    ].min) lightLevel = Dark;
    else if (lightLevel > Dark     && lux <= limits[Dark    ].max) lightLevel = Dark;
    else if (lightLevel > Twilight && lux <= limits[Twilight].max) lightLevel = Twilight;
    else if (lightLevel > Light    && lux <= limits[Light   ].max) lightLevel = Light;
    else if (lightLevel > Bright   && lux <= limits[Bright  ].max) lightLevel = Bright;

    //qDebug() << "lightLevel" << light_level(lightLevel) << "lux" << lux;

    if (static_cast<int>(m_reading.lightLevel()) != lightLevel || m_reading.timestamp() == 0) {
        m_reading.setTimestamp(reading->timestamp());
        m_reading.setLightLevel(static_cast<QAmbientLightReading::LightLevel>(lightLevel));

        newReadingAvailable();
    }

    return false;
}

