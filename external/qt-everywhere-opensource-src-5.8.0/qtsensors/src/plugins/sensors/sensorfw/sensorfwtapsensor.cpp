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

#include "sensorfwtapsensor.h"

char const * const SensorfwTapSensor::id("sensorfw.tapsensor");

SensorfwTapSensor::SensorfwTapSensor(QSensor *sensor)
    : SensorfwSensorBase(sensor),
      m_initDone(false),
      m_isOnceStarted(false)
{
    init();
    setReading<QTapReading>(&m_reading);
    addOutputRange(QTapReading::Undefined, QTapReading::Z_Both, 1);
    addDataRate(10,10); //TODO: fix this when we know better
    sensor->setDataRate(10);//set a default rate
}


void SensorfwTapSensor::start()
{
    if (reinitIsNeeded)
        init();

    QTapSensor * const tapSensor = qobject_cast<QTapSensor *>(sensor());

    bool b = tapSensor->returnDoubleTapEvents();
    bool isDoubleTapSensor = m_isDoubleTapSensor;
    if (!b) {
        tapSensor->setReturnDoubleTapEvents(true); //by default doubles
        m_isDoubleTapSensor = true;
    }
    else m_isDoubleTapSensor = b;

    if (!m_isOnceStarted || (m_isOnceStarted && isDoubleTapSensor != m_isDoubleTapSensor)) {
        TapSensorChannelInterface *iface = static_cast<TapSensorChannelInterface *>(m_sensorInterface);
        if (!iface) {
            qWarning() << "Sensor interface is not initialized";
            return;
        }
        iface->setTapType(m_isDoubleTapSensor?TapSensorChannelInterface::Double:TapSensorChannelInterface::Single);
    }

    SensorfwSensorBase::start();
    // Set tap type (single/double)
    m_reading.setDoubleTap(m_isDoubleTapSensor);
    m_isOnceStarted = true;
}


void SensorfwTapSensor::slotDataAvailable(const Tap& data)
{
    // Set tap direction
    QTapReading::TapDirection o;
    switch (data.direction()) {
    case TapData::X:         o = QTapReading::X_Both;    break;
    case TapData::Y:         o = QTapReading::Y_Both;    break;
    case TapData::Z:         o = QTapReading::Z_Both;    break;
    case TapData::LeftRight: o = QTapReading::X_Pos;     break;
    case TapData::RightLeft: o = QTapReading::X_Neg;     break;
    case TapData::TopBottom: o = QTapReading::Z_Neg;     break;
    case TapData::BottomTop: o = QTapReading::Z_Pos;     break;
    case TapData::FaceBack:  o = QTapReading::Y_Pos;     break;
    case TapData::BackFace:  o = QTapReading::Y_Neg;     break;
    default:                 o = QTapReading::Undefined;
    }
    m_reading.setTapDirection(o);
    m_reading.setTimestamp(data.tapData().timestamp_);
    newReadingAvailable();
}


bool SensorfwTapSensor::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    return QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(Tap)),
                            this, SLOT(slotDataAvailable(Tap)));
}


QString SensorfwTapSensor::sensorName() const
{
    return "tapsensor";
}

void SensorfwTapSensor::init()
{
    m_initDone = false;
    initSensor<TapSensorChannelInterface>(m_initDone);
}
