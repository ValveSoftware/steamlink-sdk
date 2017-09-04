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
    QAmbientLightReading::LightLevel level = getLightLevel(data.UnsignedData().value_);
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
    } else if (lux < 80) {
        return QAmbientLightReading::Twilight;
    } else if (lux < 400) {
        return QAmbientLightReading::Light;
    } else if (lux < 2500) {
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
