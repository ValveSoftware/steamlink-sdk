/****************************************************************************
**
** Copyright (C) 2016 Canonical, Ltd
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

#include "sensorfwlidsensor.h"

char const * const SensorfwLidSensor::id("sensorfw.lidsensor");

SensorfwLidSensor::SensorfwLidSensor(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setReading<QLidReading>(&m_reading);
    sensor->setDataRate(10);//set a default rate
}

void SensorfwLidSensor::slotDataAvailable(const LidData& data)
{
    switch (data.type_) {
    case data.BackLid:
        m_reading.setBackLidClosed(data.value_);
        break;
    case data.FrontLid:
        m_reading.setFrontLidClosed(data.value_);
        break;
    };

    m_reading.setTimestamp(data.timestamp_);
    newReadingAvailable();
}

bool SensorfwLidSensor::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return QObject::connect(m_sensorInterface, SIGNAL(lidChanged(LidData)),
                            this, SLOT(slotDataAvailable(LidData)));
}

QString SensorfwLidSensor::sensorName() const
{
    return "lidsensor";
}

void SensorfwLidSensor::init()
{
    m_initDone = false;
    initSensor<LidSensorChannelInterface>(m_initDone);
}

void SensorfwLidSensor::start()
{
    if (reinitIsNeeded)
        init();
    SensorfwSensorBase::start();
}
