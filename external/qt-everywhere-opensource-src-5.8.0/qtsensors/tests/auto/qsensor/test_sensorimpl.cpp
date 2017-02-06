/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "test_sensorimpl.h"
#include <QDebug>

const char *testsensorimpl::id("test sensor impl");

static testsensorimpl *exclusiveHandle = 0;

testsensorimpl::testsensorimpl(QSensor *sensor)
    : QSensorBackend(sensor)
{
    setReading<TestSensorReading>(&m_reading);
    setDescription("sensor description");
    addOutputRange(0, 1, 0.5);
    addOutputRange(0, 2, 1);
    QString doThis = sensor->property("doThis").toString();
    if (doThis == "rates(0)") {
        setDataRates(0);
    } else if (doThis == "rates(nodef)") {
        TestSensor *acc = new TestSensor(this);
        setDataRates(acc);
        delete acc;
    } else if (doThis == "rates") {
        TestSensor *acc = new TestSensor(this);
        acc->connectToBackend();
        setDataRates(acc);
        delete acc;
    } else {
        addDataRate(100, 100);
    }
    reading();
}

testsensorimpl::~testsensorimpl()
{
    Q_ASSERT(exclusiveHandle != this);
}

void testsensorimpl::start()
{
    QVariant _exclusive = sensor()->property("exclusive");
    bool exclusive = _exclusive.isValid()?_exclusive.toBool():false;
    if (exclusive) {
        if (!exclusiveHandle) {
            exclusiveHandle = this;
        } else {
            // Hook up the busyChanged signal
            connect(exclusiveHandle, SIGNAL(emitBusyChanged()), sensor(), SIGNAL(busyChanged()));
            sensorBusy(); // report the busy condition
            return;
        }
    }

    QString doThis = sensor()->property("doThis").toString();
    if (doThis == "stop")
        sensorStopped();
    else if (doThis == "error")
        sensorError(1);
    else if (doThis == "setOne") {
        m_reading.setTimestamp(1);
        m_reading.setTest(1);
        newReadingAvailable();
    } else {
        m_reading.setTimestamp(2);
        m_reading.setTest(2);
        newReadingAvailable();
    }
}

void testsensorimpl::stop()
{
    QVariant _exclusive = sensor()->property("exclusive");
    bool exclusive = _exclusive.isValid()?_exclusive.toBool():false;
    if (exclusive && exclusiveHandle == this) {
        exclusiveHandle = 0;
        emit emitBusyChanged(); // notify any waiting instances that they can try to grab the sensor now
    }
}

bool testsensorimpl::isFeatureSupported(QSensor::Feature feature) const
{
    return (feature == QSensor::AlwaysOn || feature == QSensor::GeoValues);
}

