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

#include <qsensorplugin.h>
#include <qsensorbackend.h>
#include <qsensormanager.h>

#include "iosmotionmanager.h"
#include "iosaccelerometer.h"
#include "iosgyroscope.h"
#include "iosmagnetometer.h"
#include "ioscompass.h"
#include "iosproximitysensor.h"

#import <CoreLocation/CoreLocation.h>
#ifdef HAVE_COREMOTION
#import <CoreMotion/CoreMotion.h>
#endif

class IOSSensorPlugin : public QObject, public QSensorPluginInterface, public QSensorBackendFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)
public:
    void registerSensors()
    {
#ifdef HAVE_COREMOTION
        QSensorManager::registerBackend(QAccelerometer::type, IOSAccelerometer::id, this);
        if ([QIOSMotionManager sharedManager].gyroAvailable)
            QSensorManager::registerBackend(QGyroscope::type, IOSGyroscope::id, this);
        if ([QIOSMotionManager sharedManager].magnetometerAvailable)
            QSensorManager::registerBackend(QMagnetometer::type, IOSMagnetometer::id, this);
#endif
#ifdef HAVE_COMPASS
        if ([CLLocationManager headingAvailable])
            QSensorManager::registerBackend(QCompass::type, IOSCompass::id, this);
#endif
#ifdef HAVE_UIDEVICE
        if (IOSProximitySensor::available())
            QSensorManager::registerBackend(QProximitySensor::type, IOSProximitySensor::id, this);
#endif
    }

    QSensorBackend *createBackend(QSensor *sensor)
    {
#ifdef HAVE_COREMOTION
        if (sensor->identifier() == IOSAccelerometer::id)
            return new IOSAccelerometer(sensor);
        if (sensor->identifier() == IOSGyroscope::id)
            return new IOSGyroscope(sensor);
        if (sensor->identifier() == IOSMagnetometer::id)
            return new IOSMagnetometer(sensor);
#endif
#ifdef HAVE_COMPASS
        if (sensor->identifier() == IOSCompass::id)
            return new IOSCompass(sensor);
#endif
#ifdef HAVE_UIDEVICE
        if (sensor->identifier() == IOSProximitySensor::id)
            return new IOSProximitySensor(sensor);
#endif
        return 0;
    }
};

#include "main.moc"

