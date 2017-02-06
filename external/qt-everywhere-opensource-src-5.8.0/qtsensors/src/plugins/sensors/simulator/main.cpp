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

#include "simulatoraccelerometer.h"
#include "simulatorambientlightsensor.h"
#include "simulatorlightsensor.h"
#include "simulatorcompass.h"
#include "simulatorproximitysensor.h"
#include "simulatorirproximitysensor.h"
#include "simulatormagnetometer.h"
#include <QSensorPluginInterface>
#include <QSensorBackend>
#include <QSensorManager>

class SimulatorSensorPlugin : public QObject, public QSensorPluginInterface, public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)
public:
    SimulatorSensorPlugin()
    {
        SensorsConnection *connection = SensorsConnection::instance();
        if (!connection) return; // hardly likely but just in case...
        connect(connection, SIGNAL(setAvailableFeatures(quint32)), this, SLOT(setAvailableFeatures(quint32)));
    }

    void registerSensors()
    {
        QSensorManager::registerBackend(QAccelerometer::type, SimulatorAccelerometer::id, this);
        QSensorManager::registerBackend(QAmbientLightSensor::type, SimulatorAmbientLightSensor::id, this);
        QSensorManager::registerBackend(QLightSensor::type, SimulatorLightSensor::id, this);
        QSensorManager::registerBackend(QCompass::type, SimulatorCompass::id, this);
        QSensorManager::registerBackend(QProximitySensor::type, SimulatorProximitySensor::id, this);
        QSensorManager::registerBackend(QIRProximitySensor::type, SimulatorIRProximitySensor::id, this);
        QSensorManager::registerBackend(QMagnetometer::type, SimulatorMagnetometer::id, this);
    }

    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == SimulatorAccelerometer::id) {
            return new SimulatorAccelerometer(sensor);
        }

        if (sensor->identifier() == SimulatorAmbientLightSensor::id) {
            return new SimulatorAmbientLightSensor(sensor);
        }

        if (sensor->identifier() == SimulatorLightSensor::id) {
            return new SimulatorLightSensor(sensor);
        }

        if (sensor->identifier() == SimulatorProximitySensor::id) {
            return new SimulatorProximitySensor(sensor);
        }

        if (sensor->identifier() == SimulatorIRProximitySensor::id) {
            return new SimulatorIRProximitySensor(sensor);
        }

        if (sensor->identifier() == SimulatorCompass::id) {
            return new SimulatorCompass(sensor);
        }

        if (sensor->identifier() == SimulatorMagnetometer::id) {
            return new SimulatorMagnetometer(sensor);
        }

        return 0;
    }

    // Copied from the emulator codebase
    enum Features {
        Accelerometer  = 0x01,
        Magnetometer   = 0x02,
        Compass        = 0x04,
        Infraredsensor = 0x08,
        Lightsensor    = 0x10
    };

public slots:
    void setAvailableFeatures(quint32 features)
    {
        check(features&Accelerometer,  QAccelerometer::type,      SimulatorAccelerometer::id);
        check(features&Lightsensor,    QLightSensor::type,        SimulatorLightSensor::id);
        check(features&Lightsensor,    QAmbientLightSensor::type, SimulatorAmbientLightSensor::id);
        check(features&Magnetometer,   QMagnetometer::type,       SimulatorMagnetometer::id);
        check(features&Compass,        QCompass::type,            SimulatorCompass::id);
        check(features&Infraredsensor, QIRProximitySensor::type,  SimulatorIRProximitySensor::id);
        check(features&Infraredsensor, QProximitySensor::type,    SimulatorProximitySensor::id);
    }

private:
    void check(bool test, const QByteArray &type, const QByteArray &id)
    {
        if (test) {
            if (!QSensorManager::isBackendRegistered(type, id))
                QSensorManager::registerBackend(type, id, this);
        } else {
            if (QSensorManager::isBackendRegistered(type, id))
                QSensorManager::unregisterBackend(type, id);
        }
    }
};

#include "main.moc"

