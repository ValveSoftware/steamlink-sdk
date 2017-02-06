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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtSensors/QSensorManager>

#include <QtSensors/qaccelerometer.h>
#include <QtSensors/qaltimeter.h>
#include <QtSensors/qambientlightsensor.h>
#include <QtSensors/qambienttemperaturesensor.h>
#include <QtSensors/qcompass.h>
#include <QtSensors/qmagnetometer.h>
#include <QtSensors/qorientationsensor.h>
#include <QtSensors/qproximitysensor.h>
#include <QtSensors/qrotationsensor.h>
#include <QtSensors/qtapsensor.h>
#include <QtSensors/qlightsensor.h>
#include <QtSensors/qgyroscope.h>
#include <QtSensors/qirproximitysensor.h>
#include <QtSensors/qtiltsensor.h>

#include "qmlsensorglobal.h"
#include "qmlsensor.h"
#include "qmlaccelerometer.h"
#include "qmlaltimeter.h"
#include "qmlambientlightsensor.h"
#include "qmlambienttemperaturesensor.h"
#include "qmlcompass.h"
#include "qmlgyroscope.h"
#include "qmlholstersensor.h"
#include "qmlirproximitysensor.h"
#include "qmllightsensor.h"
#include "qmlmagnetometer.h"
#include "qmlorientationsensor.h"
#include "qmlpressuresensor.h"
#include "qmlproximitysensor.h"
#include "qmlrotationsensor.h"
#include "qmltapsensor.h"
#include "qmltiltsensor.h"
#include "qmlsensorgesture.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtSensors);
#endif
}

QT_BEGIN_NAMESPACE

static QObject *global_object_50(QQmlEngine *, QJSEngine *)
{
    return new QmlSensorGlobal;
}

class QtSensorsDeclarativeModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid FILE "plugin.json")
public:
    QtSensorsDeclarativeModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        char const * const package = "QtSensors";
        if (QLatin1String(uri) != QLatin1String(package)) return;
        int major;
        int minor;

        // Register the 5.0 interfaces
        major = 5;
        minor = 0;
        qmlRegisterSingletonType  <QmlSensorGlobal             >(package, major, minor, "QmlSensors", global_object_50);
        qmlRegisterUncreatableType<QmlSensorRange              >(package, major, minor, "Range",                QLatin1String("Cannot create Range"));
        qmlRegisterUncreatableType<QmlSensorOutputRange        >(package, major, minor, "OutputRange",          QLatin1String("Cannot create OutputRange"));
        qmlRegisterUncreatableType<QmlSensor                   >(package, major, minor, "Sensor",               QLatin1String("Cannot create Sensor"));
        qmlRegisterUncreatableType<QmlSensorReading            >(package, major, minor, "SensorReading",        QLatin1String("Cannot create SensorReading"));
        qmlRegisterType           <QmlAccelerometer            >(package, major, minor, "Accelerometer");
        qmlRegisterUncreatableType<QmlAccelerometerReading     >(package, major, minor, "AccelerometerReading", QLatin1String("Cannot create AccelerometerReading"));
        qmlRegisterType           <QmlAmbientLightSensor       >(package, major, minor, "AmbientLightSensor");
        qmlRegisterUncreatableType<QmlAmbientLightSensorReading>(package, major, minor, "AmbientLightReading",  QLatin1String("Cannot create AmbientLightReading"));
        qmlRegisterType           <QmlCompass                  >(package, major, minor, "Compass");
        qmlRegisterUncreatableType<QmlCompassReading           >(package, major, minor, "CompassReading",       QLatin1String("Cannot create CompassReading"));
        qmlRegisterType           <QmlGyroscope                >(package, major, minor, "Gyroscope");
        qmlRegisterUncreatableType<QmlGyroscopeReading         >(package, major, minor, "GyroscopeReading",     QLatin1String("Cannot create GyroscopeReading"));
        qmlRegisterType           <QmlIRProximitySensor        >(package, major, minor, "IRProximitySensor");
        qmlRegisterUncreatableType<QmlIRProximitySensorReading >(package, major, minor, "IRProximityReading",   QLatin1String("Cannot create IRProximityReading"));
        qmlRegisterType           <QmlLightSensor              >(package, major, minor, "LightSensor");
        qmlRegisterUncreatableType<QmlLightSensorReading       >(package, major, minor, "LightReading",         QLatin1String("Cannot create LightReading"));
        qmlRegisterType           <QmlMagnetometer             >(package, major, minor, "Magnetometer");
        qmlRegisterUncreatableType<QmlMagnetometerReading      >(package, major, minor, "MagnetometerReading",  QLatin1String("Cannot create MagnetometerReading"));
        qmlRegisterType           <QmlOrientationSensor        >(package, major, minor, "OrientationSensor");
        qmlRegisterUncreatableType<QmlOrientationSensorReading >(package, major, minor, "OrientationReading",   QLatin1String("Cannot create OrientationReading"));
        qmlRegisterType           <QmlProximitySensor          >(package, major, minor, "ProximitySensor");
        qmlRegisterUncreatableType<QmlProximitySensorReading   >(package, major, minor, "ProximityReading",     QLatin1String("Cannot create ProximityReading"));
        qmlRegisterType           <QmlRotationSensor           >(package, major, minor, "RotationSensor");
        qmlRegisterUncreatableType<QmlRotationSensorReading    >(package, major, minor, "RotationReading",      QLatin1String("Cannot create RotationReading"));
        qmlRegisterType           <QmlTapSensor                >(package, major, minor, "TapSensor");
        qmlRegisterUncreatableType<QmlTapSensorReading         >(package, major, minor, "TapReading",           QLatin1String("Cannot create TapReading"));
        qmlRegisterType           <QmlTiltSensor               >(package, major, minor, "TiltSensor");
        qmlRegisterUncreatableType<QmlTiltSensorReading        >(package, major, minor, "TiltReading",          QLatin1String("Cannot create TiltReading"));

        qmlRegisterType           <QmlSensorGesture            >(package, major, minor, "SensorGesture");

        // Register the 5.1 interfaces
        minor = 1;
        qmlRegisterSingletonType  <QmlSensorGlobal             >(package, major, minor, "QmlSensors", global_object_50);
        qmlRegisterUncreatableType<QmlSensorRange              >(package, major, minor, "Range",                QLatin1String("Cannot create Range"));
        qmlRegisterUncreatableType<QmlSensorOutputRange        >(package, major, minor, "OutputRange",          QLatin1String("Cannot create OutputRange"));
        qmlRegisterUncreatableType<QmlSensor,1                 >(package, major, minor, "Sensor",               QLatin1String("Cannot create Sensor"));
        qmlRegisterUncreatableType<QmlSensorReading            >(package, major, minor, "SensorReading",        QLatin1String("Cannot create SensorReading"));
        qmlRegisterType           <QmlAccelerometer,1          >(package, major, minor, "Accelerometer");
        qmlRegisterUncreatableType<QmlAccelerometerReading     >(package, major, minor, "AccelerometerReading", QLatin1String("Cannot create AccelerometerReading"));
        qmlRegisterType           <QmlAltimeter                >(package, major, minor, "Altimeter");
        qmlRegisterUncreatableType<QmlAltimeterReading         >(package, major, minor, "AltimeterReading", QLatin1String("Cannot create AltimeterReading"));
        qmlRegisterType           <QmlAmbientLightSensor       >(package, major, minor, "AmbientLightSensor");
        qmlRegisterUncreatableType<QmlAmbientLightSensorReading>(package, major, minor, "AmbientLightReading",  QLatin1String("Cannot create AmbientLightReading"));
        qmlRegisterType           <QmlAmbientTemperatureSensor >(package, major, minor, "AmbientTemperatureSensor");
        qmlRegisterUncreatableType<QmlAmbientTemperatureReading>(package, major, minor, "AmbientTemperatureReading",  QLatin1String("Cannot create AmbientTemperatureReading"));
        qmlRegisterType           <QmlCompass                  >(package, major, minor, "Compass");
        qmlRegisterUncreatableType<QmlCompassReading           >(package, major, minor, "CompassReading",       QLatin1String("Cannot create CompassReading"));
        qmlRegisterType           <QmlGyroscope                >(package, major, minor, "Gyroscope");
        qmlRegisterUncreatableType<QmlGyroscopeReading         >(package, major, minor, "GyroscopeReading",     QLatin1String("Cannot create GyroscopeReading"));
        qmlRegisterType           <QmlHolsterSensor            >(package, major, minor, "HolsterSensor");
        qmlRegisterUncreatableType<QmlHolsterReading           >(package, major, minor, "HolsterReading",       QLatin1String("Cannot create HolsterReading"));
        qmlRegisterType           <QmlIRProximitySensor        >(package, major, minor, "IRProximitySensor");
        qmlRegisterUncreatableType<QmlIRProximitySensorReading >(package, major, minor, "IRProximityReading",   QLatin1String("Cannot create IRProximityReading"));
        qmlRegisterType           <QmlLightSensor              >(package, major, minor, "LightSensor");
        qmlRegisterUncreatableType<QmlLightSensorReading       >(package, major, minor, "LightReading",         QLatin1String("Cannot create LightReading"));
        qmlRegisterType           <QmlMagnetometer             >(package, major, minor, "Magnetometer");
        qmlRegisterUncreatableType<QmlMagnetometerReading      >(package, major, minor, "MagnetometerReading",  QLatin1String("Cannot create MagnetometerReading"));
        qmlRegisterType           <QmlOrientationSensor        >(package, major, minor, "OrientationSensor");
        qmlRegisterUncreatableType<QmlOrientationSensorReading >(package, major, minor, "OrientationReading",   QLatin1String("Cannot create OrientationReading"));
        qmlRegisterType           <QmlPressureSensor           >(package, major, minor, "PressureSensor");
        qmlRegisterUncreatableType<QmlPressureReading          >(package, major, minor, "PressureReading",      QLatin1String("Cannot create PressureReading"));
        qmlRegisterType           <QmlProximitySensor          >(package, major, minor, "ProximitySensor");
        qmlRegisterUncreatableType<QmlProximitySensorReading   >(package, major, minor, "ProximityReading",     QLatin1String("Cannot create ProximityReading"));
        qmlRegisterType           <QmlRotationSensor           >(package, major, minor, "RotationSensor");
        qmlRegisterUncreatableType<QmlRotationSensorReading    >(package, major, minor, "RotationReading",      QLatin1String("Cannot create RotationReading"));
        qmlRegisterType           <QmlTapSensor                >(package, major, minor, "TapSensor");
        qmlRegisterUncreatableType<QmlTapSensorReading         >(package, major, minor, "TapReading",           QLatin1String("Cannot create TapReading"));
        qmlRegisterType           <QmlTiltSensor               >(package, major, minor, "TiltSensor");
        qmlRegisterUncreatableType<QmlTiltSensorReading        >(package, major, minor, "TiltReading",          QLatin1String("Cannot create TiltReading"));

        qmlRegisterType           <QmlSensorGesture            >(package, major, minor, "SensorGesture");

        // Register the 5.2 interfaces
        minor = 2;
        qmlRegisterSingletonType  <QmlSensorGlobal             >(package, major, minor, "QmlSensors", global_object_50);
        qmlRegisterUncreatableType<QmlSensorRange              >(package, major, minor, "Range",                QLatin1String("Cannot create Range"));
        qmlRegisterUncreatableType<QmlSensorOutputRange        >(package, major, minor, "OutputRange",          QLatin1String("Cannot create OutputRange"));
        qmlRegisterUncreatableType<QmlSensor,1                 >(package, major, minor, "Sensor",               QLatin1String("Cannot create Sensor"));
        qmlRegisterUncreatableType<QmlSensorReading            >(package, major, minor, "SensorReading",        QLatin1String("Cannot create SensorReading"));
        qmlRegisterType           <QmlAccelerometer,1          >(package, major, minor, "Accelerometer");
        qmlRegisterUncreatableType<QmlAccelerometerReading     >(package, major, minor, "AccelerometerReading", QLatin1String("Cannot create AccelerometerReading"));
        qmlRegisterType           <QmlAltimeter                >(package, major, minor, "Altimeter");
        qmlRegisterUncreatableType<QmlAltimeterReading         >(package, major, minor, "AltimeterReading", QLatin1String("Cannot create AltimeterReading"));
        qmlRegisterType           <QmlAmbientLightSensor       >(package, major, minor, "AmbientLightSensor");
        qmlRegisterUncreatableType<QmlAmbientLightSensorReading>(package, major, minor, "AmbientLightReading",  QLatin1String("Cannot create AmbientLightReading"));
        qmlRegisterType           <QmlAmbientTemperatureSensor >(package, major, minor, "AmbientTemperatureSensor");
        qmlRegisterUncreatableType<QmlAmbientTemperatureReading>(package, major, minor, "AmbientTemperatureReading",  QLatin1String("Cannot create AmbientTemperatureReading"));
        qmlRegisterType           <QmlCompass                  >(package, major, minor, "Compass");
        qmlRegisterUncreatableType<QmlCompassReading           >(package, major, minor, "CompassReading",       QLatin1String("Cannot create CompassReading"));
        qmlRegisterType           <QmlGyroscope                >(package, major, minor, "Gyroscope");
        qmlRegisterUncreatableType<QmlGyroscopeReading         >(package, major, minor, "GyroscopeReading",     QLatin1String("Cannot create GyroscopeReading"));
        qmlRegisterType           <QmlHolsterSensor            >(package, major, minor, "HolsterSensor");
        qmlRegisterUncreatableType<QmlHolsterReading           >(package, major, minor, "HolsterReading",       QLatin1String("Cannot create HolsterReading"));
        qmlRegisterType           <QmlIRProximitySensor        >(package, major, minor, "IRProximitySensor");
        qmlRegisterUncreatableType<QmlIRProximitySensorReading >(package, major, minor, "IRProximityReading",   QLatin1String("Cannot create IRProximityReading"));
        qmlRegisterType           <QmlLightSensor              >(package, major, minor, "LightSensor");
        qmlRegisterUncreatableType<QmlLightSensorReading       >(package, major, minor, "LightReading",         QLatin1String("Cannot create LightReading"));
        qmlRegisterType           <QmlMagnetometer             >(package, major, minor, "Magnetometer");
        qmlRegisterUncreatableType<QmlMagnetometerReading      >(package, major, minor, "MagnetometerReading",  QLatin1String("Cannot create MagnetometerReading"));
        qmlRegisterType           <QmlOrientationSensor        >(package, major, minor, "OrientationSensor");
        qmlRegisterUncreatableType<QmlOrientationSensorReading >(package, major, minor, "OrientationReading",   QLatin1String("Cannot create OrientationReading"));
        qmlRegisterType           <QmlPressureSensor           >(package, major, minor, "PressureSensor");
        qmlRegisterUncreatableType<QmlPressureReading,1        >(package, major, minor, "PressureReading",      QLatin1String("Cannot create PressureReading"));
        qmlRegisterType           <QmlProximitySensor          >(package, major, minor, "ProximitySensor");
        qmlRegisterUncreatableType<QmlProximitySensorReading   >(package, major, minor, "ProximityReading",     QLatin1String("Cannot create ProximityReading"));
        qmlRegisterType           <QmlRotationSensor           >(package, major, minor, "RotationSensor");
        qmlRegisterUncreatableType<QmlRotationSensorReading    >(package, major, minor, "RotationReading",      QLatin1String("Cannot create RotationReading"));
        qmlRegisterType           <QmlTapSensor                >(package, major, minor, "TapSensor");
        qmlRegisterUncreatableType<QmlTapSensorReading         >(package, major, minor, "TapReading",           QLatin1String("Cannot create TapReading"));
        qmlRegisterType           <QmlTiltSensor               >(package, major, minor, "TiltSensor");
        qmlRegisterUncreatableType<QmlTiltSensorReading        >(package, major, minor, "TiltReading",          QLatin1String("Cannot create TiltReading"));

        qmlRegisterType           <QmlSensorGesture            >(package, major, minor, "SensorGesture");

        // Register the 5.8 interfaces
        // No API changes, just reintroduce existing interfaces from 5.2
        // Implicitly registers 5.3 - 5.7 too
        minor = 8;
        qmlRegisterType           <QmlAltimeter                >(package, major, minor, "Altimeter");
    }
};

QT_END_NAMESPACE

#include "sensors.moc"
