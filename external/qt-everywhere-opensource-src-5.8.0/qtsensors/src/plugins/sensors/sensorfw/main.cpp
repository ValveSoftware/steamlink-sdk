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

#include "sensorfwaccelerometer.h"
#include "sensorfwals.h"
#include "sensorfwcompass.h"
#include "sensorfwmagnetometer.h"
#include "sensorfworientationsensor.h"
#include "sensorfwproximitysensor.h"
#include "sensorfwirproximitysensor.h"
#include "sensorfwrotationsensor.h"
#include "sensorfwtapsensor.h"
#include "sensorfwgyroscope.h"
#include "sensorfwlightsensor.h"

#include <QtSensors/qsensorplugin.h>
#include <QtSensors/qsensorbackend.h>
#include <QtSensors/qsensormanager.h>
#include <QDebug>
#include <QSettings>

class sensorfwSensorPlugin : public QObject, public QSensorPluginInterface, public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)

public:

    void registerSensors()
    {
        // if no default - no support either, uses Sensors.conf
        QSettings settings(QSettings::SystemScope, QLatin1String("QtProject"), QLatin1String("Sensors"));
        settings.beginGroup(QLatin1String("Default"));
        QStringList keys = settings.allKeys();
        for (int i=0,l=keys.size(); i<l; i++) {
            QString type = keys.at(i);
            if (settings.value(type).toString().contains(QStringLiteral("sensorfw")))//register only ones we know
                QSensorManager::registerBackend(type.toLocal8Bit(), settings.value(type).toByteArray(), this);
        }
    }


    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == sensorfwaccelerometer::id)
            return new sensorfwaccelerometer(sensor);
        if (sensor->identifier() == Sensorfwals::id)
            return new Sensorfwals(sensor);
        if (sensor->identifier() == SensorfwCompass::id)
            return new SensorfwCompass(sensor);
        if (sensor->identifier() == SensorfwMagnetometer::id)
            return new SensorfwMagnetometer(sensor);
        if (sensor->identifier() == SensorfwOrientationSensor::id)
            return new SensorfwOrientationSensor(sensor);
        if (sensor->identifier() == SensorfwProximitySensor::id)
            return new SensorfwProximitySensor(sensor);
        if (sensor->identifier() == SensorfwRotationSensor::id)
            return new SensorfwRotationSensor(sensor);
        if (sensor->identifier() == SensorfwTapSensor::id)
            return new SensorfwTapSensor(sensor);
        if (sensor->identifier() == SensorfwGyroscope::id)
            return new SensorfwGyroscope(sensor);
        if (sensor->identifier() == SensorfwLightSensor::id)
            return new SensorfwLightSensor(sensor);
        if (sensor->identifier() == SensorfwIrProximitySensor::id)
            return new SensorfwIrProximitySensor(sensor);
        return 0;
    }
};

#include "main.moc"
