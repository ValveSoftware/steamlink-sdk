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

#ifndef TEST_BACKENDS_H
#define TEST_BACKENDS_H

#include <qsensorbackend.h>

void register_test_backends();
void unregister_test_backends();

#include <qaccelerometer.h>
#include <qaltimeter.h>
#include <qambientlightsensor.h>
#include <qambienttemperaturesensor.h>
#include <qcompass.h>
#include <qgyroscope.h>
#include <qholstersensor.h>
#include <qlightsensor.h>
#include <qmagnetometer.h>
#include <qorientationsensor.h>
#include <qpressuresensor.h>
#include <qproximitysensor.h>
#include <qrotationsensor.h>
#include <qtapsensor.h>
#include <qirproximitysensor.h>
#include <qtiltsensor.h>

#define PREPARE_SENSORINTERFACE_DECLS(SensorClass, ReadingClass, FilterClass, readingcode)\
    class SensorClass ## _impl : public QSensorBackend\
    {\
    public:\
        SensorClass ## _impl(QSensor *sensor);\
        void start();\
        void stop();\
    };\
    class SensorClass ## _testfilter : public FilterClass { bool filter(ReadingClass *); };

#define PREPARE_SENSORINTERFACE_IMPLS(SensorClass, ReadingClass, FilterClass, readingcode)\
    SensorClass ## _impl::SensorClass ##_impl(QSensor *sensor) : QSensorBackend(sensor) {}\
    void SensorClass ## _impl::start() {\
        ReadingClass *reading = setReading<ReadingClass>(0);\
        readingcode\
        newReadingAvailable();\
    }\
    void SensorClass ##_impl::stop() {}\
    bool SensorClass ## _testfilter::filter(ReadingClass *) { return true; }\
    static QSensorBackend *create_ ## SensorClass ## _impl(QSensor *sensor) { return new SensorClass ## _impl(sensor); }\
    static bool registered_ ## SensorClass = registerTestBackend(#SensorClass, create_ ## SensorClass ## _impl);

#ifdef REGISTER_TOO
#define PREPARE_SENSORINTERFACE(SensorClass, ReadingClass, FilterClass, readingcode)\
        PREPARE_SENSORINTERFACE_DECLS(SensorClass, ReadingClass, FilterClass, readingcode)\
        PREPARE_SENSORINTERFACE_IMPLS(SensorClass, ReadingClass, FilterClass, readingcode)
#else
#define PREPARE_SENSORINTERFACE(SensorClass, ReadingClass, FilterClass, readingcode)\
        PREPARE_SENSORINTERFACE_DECLS(SensorClass, ReadingClass, FilterClass, readingcode)
#endif

PREPARE_SENSORINTERFACE(QAccelerometer, QAccelerometerReading, QAccelerometerFilter, {
    reading->setX(1.0);
    reading->setY(1.0);
    reading->setZ(1.0);
})
PREPARE_SENSORINTERFACE(QAltimeter, QAltimeterReading, QAltimeterFilter, {
    reading->setAltitude(8848);
})
PREPARE_SENSORINTERFACE(QAmbientLightSensor, QAmbientLightReading, QAmbientLightFilter, {
    reading->setLightLevel(QAmbientLightReading::Twilight);
})
PREPARE_SENSORINTERFACE(QAmbientTemperatureSensor, QAmbientTemperatureReading, QAmbientTemperatureFilter, {
    reading->setTemperature(30);
})
PREPARE_SENSORINTERFACE(QCompass, QCompassReading, QCompassFilter, {
    reading->setAzimuth(1.0);
    reading->setCalibrationLevel(1.0);
})
PREPARE_SENSORINTERFACE(QGyroscope, QGyroscopeReading, QGyroscopeFilter, {
    reading->setX(1.0);
    reading->setY(1.0);
    reading->setZ(1.0);
})
PREPARE_SENSORINTERFACE(QHolsterSensor, QHolsterReading, QHolsterFilter, {
    reading->setHolstered(true);
})
PREPARE_SENSORINTERFACE(QLightSensor, QLightReading, QLightFilter, {
    reading->setLux(1.0);
})
PREPARE_SENSORINTERFACE(QMagnetometer, QMagnetometerReading, QMagnetometerFilter, {
    reading->setX(1.0);
    reading->setY(1.0);
    reading->setZ(1.0);
    reading->setCalibrationLevel(1.0);
})
PREPARE_SENSORINTERFACE(QOrientationSensor, QOrientationReading, QOrientationFilter, {
    reading->setOrientation(QOrientationReading::LeftUp);
})
PREPARE_SENSORINTERFACE(QPressureSensor, QPressureReading, QPressureFilter, {
    reading->setPressure(1.0);
    reading->setTemperature(1.0);
})
PREPARE_SENSORINTERFACE(QProximitySensor, QProximityReading, QProximityFilter, {
    reading->setClose(true);
})
PREPARE_SENSORINTERFACE(QRotationSensor, QRotationReading, QRotationFilter, {
    reading->setFromEuler(1.0, 1.0, 1.0);
})
PREPARE_SENSORINTERFACE(QTapSensor, QTapReading, QTapFilter, {
    reading->setTapDirection(QTapReading::Z_Both);
    reading->setDoubleTap(true);
})
PREPARE_SENSORINTERFACE(QIRProximitySensor, QIRProximityReading, QIRProximityFilter, {
    reading->setReflectance(0.5);
})
PREPARE_SENSORINTERFACE(QTiltSensor, QTiltReading, QTiltFilter, {
    reading->setYRotation(1.0);
    reading->setXRotation(1.0);
})

#define TEST_SENSORINTERFACE(SensorClass, ReadingClass, readingcode)\
    do {\
        SensorClass sensor;\
        SensorClass ## _testfilter filter;\
        sensor.addFilter(&filter);\
        sensor.start();\
        ReadingClass *reading = sensor.reading();\
        readingcode\
    } while (0);

#endif
