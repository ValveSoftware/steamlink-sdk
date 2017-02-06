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

#include "sensorfwproximitysensor.h"

char const * const SensorfwProximitySensor::id("sensorfw.proximitysensor");

SensorfwProximitySensor::SensorfwProximitySensor(QSensor *sensor)
    : SensorfwSensorBase(sensor),
      m_initDone(false),
      m_exClose(false),
      firstRun(true)
{
    init();
    setReading<QProximityReading>(&m_reading);
    addDataRate(10,10); //TODO: fix this when we know better
    sensor->setDataRate(10);//set a default rate
}

void SensorfwProximitySensor::start()
{
    if (reinitIsNeeded)
        init();
    SensorfwSensorBase::start();
}


void SensorfwProximitySensor::slotDataAvailable(const Unsigned& data)
{
    bool close = data.x()? true: false;
    if (!firstRun && close == m_exClose) return;
    m_reading.setClose(close);
    m_reading.setTimestamp(data.UnsignedData().timestamp_);
    newReadingAvailable();
    m_exClose = close;
    if (firstRun)
        firstRun = false;
}

bool SensorfwProximitySensor::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return (QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(Unsigned)),
                             this, SLOT(slotDataAvailable(Unsigned))));
}


QString SensorfwProximitySensor::sensorName() const
{
    return "proximitysensor";
}

void SensorfwProximitySensor::init()
{
    m_initDone = false;
    initSensor<ProximitySensorChannelInterface>(m_initDone);
}
