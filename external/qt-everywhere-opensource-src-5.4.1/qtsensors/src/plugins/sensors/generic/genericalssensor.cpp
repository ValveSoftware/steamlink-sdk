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

