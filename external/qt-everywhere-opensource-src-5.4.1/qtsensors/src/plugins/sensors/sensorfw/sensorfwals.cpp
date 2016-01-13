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


#include "sensorfwals.h"

char const * const Sensorfwals::id("sensorfw.als");

Sensorfwals::Sensorfwals(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setReading<QAmbientLightReading>(&m_reading);
    // metadata
    setDescription(QLatin1String("ambient light intensity given as 5 pre-defined levels"));
    addOutputRange(0, 5, 1);
    addDataRate(10,10);
    sensor->setDataRate(10);//set a default rate
}

void Sensorfwals::start()
{
    if (reinitIsNeeded)
        init();
    if (m_sensorInterface) {
        Unsigned data(((ALSSensorChannelInterface*)m_sensorInterface)->lux());
        m_reading.setLightLevel(getLightLevel(data.x()));
        m_reading.setTimestamp(data.UnsignedData().timestamp_);
        newReadingAvailable();
    }
    SensorfwSensorBase::start();
}


void Sensorfwals::slotDataAvailable(const Unsigned& data)
{
    QAmbientLightReading::LightLevel level = getLightLevel(data.x());
    if (level != m_reading.lightLevel()) {
        m_reading.setLightLevel(level);
        m_reading.setTimestamp(data.UnsignedData().timestamp_);
        newReadingAvailable();
    }
}

bool Sensorfwals::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return QObject::connect(m_sensorInterface, SIGNAL(ALSChanged(Unsigned)),
                            this, SLOT(slotDataAvailable(Unsigned)));
}


QString Sensorfwals::sensorName() const
{
    return "alssensor";
}


QAmbientLightReading::LightLevel Sensorfwals::getLightLevel(int lux)
{
    // Convert from integer to fixed levels
    if (lux < 0) {
        return QAmbientLightReading::Undefined;
    } else if (lux < 10) {
        return QAmbientLightReading::Dark;
    } else if (lux < 50) {
        return QAmbientLightReading::Twilight;
    } else if (lux < 100) {
        return QAmbientLightReading::Light;
    } else if (lux < 150) {
        return QAmbientLightReading::Bright;
    } else {
        return QAmbientLightReading::Sunny;
    }

}
void Sensorfwals::init()
{
    m_initDone = false;
    initSensor<ALSSensorChannelInterface>(m_initDone);
}
