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

#include "sensorfworientationsensor.h"

#include <datatypes/posedata.h>

char const * const SensorfwOrientationSensor::id("sensorfw.orientationsensor");

SensorfwOrientationSensor::SensorfwOrientationSensor(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setReading<QOrientationReading>(&m_reading);
    sensor->setDataRate(10);//set a default rate
}


void SensorfwOrientationSensor::start()
{
    if (reinitIsNeeded)
        init();
    if (m_sensorInterface) {
        Unsigned data(((OrientationSensorChannelInterface*)m_sensorInterface)->orientation());
        m_reading.setOrientation(SensorfwOrientationSensor::getOrientation(data.x()));
        m_reading.setTimestamp(data.UnsignedData().timestamp_);
        newReadingAvailable();
    }
    SensorfwSensorBase::start();
}


void SensorfwOrientationSensor::slotDataAvailable(const Unsigned& data)
{
    m_reading.setOrientation(SensorfwOrientationSensor::getOrientation(data.x()));
    m_reading.setTimestamp(data.UnsignedData().timestamp_);
    newReadingAvailable();
}

bool SensorfwOrientationSensor::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return QObject::connect(m_sensorInterface, SIGNAL(orientationChanged(Unsigned)),
                            this, SLOT(slotDataAvailable(Unsigned)));
}

QString SensorfwOrientationSensor::sensorName() const
{
    return "orientationsensor";
}

QOrientationReading::Orientation SensorfwOrientationSensor::getOrientation(int orientation)
{
    switch (orientation) {
    case PoseData::BottomDown: return QOrientationReading::TopUp;
    case PoseData::BottomUp:   return QOrientationReading::TopDown;
    case PoseData::LeftUp:     return QOrientationReading::LeftUp;
    case PoseData::RightUp:    return QOrientationReading::RightUp;
    case PoseData::FaceUp:     return QOrientationReading::FaceUp;
    case PoseData::FaceDown:   return QOrientationReading::FaceDown;
    }
    return QOrientationReading::Undefined;
}

void SensorfwOrientationSensor::init()
{
    m_initDone = false;
    initSensor<OrientationSensorChannelInterface>(m_initDone);
}
