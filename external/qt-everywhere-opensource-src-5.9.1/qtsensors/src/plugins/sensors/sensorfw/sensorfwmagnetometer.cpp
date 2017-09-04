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

#include "sensorfwmagnetometer.h"


char const * const SensorfwMagnetometer::id("sensorfw.magnetometer");
const float SensorfwMagnetometer::NANO = 0.000000001;


SensorfwMagnetometer::SensorfwMagnetometer(QSensor *sensor)
    : SensorfwSensorBase(sensor)
    , m_initDone(false)
{
    init();
    setDescription(QLatin1String("magnetic flux density in teslas (T)"));
    setRanges(NANO);
    setReading<QMagnetometerReading>(&m_reading);
    sensor->setDataRate(50);//set a default rate
}

void SensorfwMagnetometer::start()
{
    if (reinitIsNeeded)
        init();
    QMagnetometer *const magnetometer = qobject_cast<QMagnetometer *>(sensor());
    if (magnetometer)
        m_isGeoMagnetometer = magnetometer->returnGeoValues();
    SensorfwSensorBase::start();
}

void SensorfwMagnetometer::slotDataAvailable(const MagneticField& data)
{
    //nanoTeslas given, divide with 10^9 to get Teslas
    m_reading.setX( NANO * (m_isGeoMagnetometer?data.x():data.rx()));
    m_reading.setY( NANO * (m_isGeoMagnetometer?data.y():data.ry()));
    m_reading.setZ( NANO * (m_isGeoMagnetometer?data.z():data.rz()));
    m_reading.setCalibrationLevel(m_isGeoMagnetometer?((float) data.level()) / 3.0 :1);
    m_reading.setTimestamp(data.timestamp());
    newReadingAvailable();
}


void SensorfwMagnetometer::slotFrameAvailable(const QVector<MagneticField>& frame)
{
    for (int i=0, l=frame.size(); i<l; i++) {
        slotDataAvailable(frame.at(i));
    }
}

bool SensorfwMagnetometer::doConnect()
{
    Q_ASSERT(m_sensorInterface);
    if (m_bufferSize==1)
        return QObject::connect(m_sensorInterface, SIGNAL(dataAvailable(MagneticField)),
                                this, SLOT(slotDataAvailable(MagneticField)));
     return QObject::connect(m_sensorInterface, SIGNAL(frameAvailable(QVector<MagneticField>)),
                             this, SLOT(slotFrameAvailable(QVector<MagneticField>)));
}

QString SensorfwMagnetometer::sensorName() const
{
    return "magnetometersensor";
}

qreal SensorfwMagnetometer::correctionFactor() const
{
    return SensorfwMagnetometer::NANO;
}

void SensorfwMagnetometer::init()
{
    m_initDone = false;
    initSensor<MagnetometerSensorChannelInterface>(m_initDone);
}
