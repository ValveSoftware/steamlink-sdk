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

#ifndef MOCKBACKENDS_H
#define MOCKBACKENDS_H

#include "mockcommon.h"

#include <qsensorplugin.h>
#include <qsensorbackend.h>
#include <qsensormanager.h>

#include <QAccelerometer>
#include <QOrientationSensor>
#include <QIRProximitySensor>
#include <QProximitySensor>

#include <QFile>
#include <QDebug>
#include <QTest>


class mockSensorPlugin : public QObject,
                         public QSensorPluginInterface,
                         public QSensorBackendFactory
{
    Q_OBJECT
  //  Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QSensorPluginInterface/1.0" FILE "plugin.json")
    Q_INTERFACES(QSensorPluginInterface)
public:
    QString m_filename;

    void registerSensors()
    {
        qDebug() << "loaded the mock plugin";

        QSensorManager::registerBackend("QAccelerometer", mockaccelerometer::id, this);
        QSensorManager::registerBackend("QIRProximitySensor", mockirproximitysensor::id, this);
        QSensorManager::registerBackend("QOrientationSensor", mockorientationsensor::id, this);
        QSensorManager::registerBackend("QTapSensor", mocktapsensor::id, this);
        QSensorManager::registerBackend("QProximitySensor", mockproximitysensor::id, this);
    }

    void unregisterSensors()
    {
        QSensorManager::unregisterBackend("QAccelerometer", mockaccelerometer::id);
        QSensorManager::unregisterBackend("QIRProximitySensor", mockirproximitysensor::id);
        QSensorManager::unregisterBackend("QOrientationSensor", mockorientationsensor::id);
        QSensorManager::unregisterBackend("QTapSensor", mocktapsensor::id);
        QSensorManager::unregisterBackend("QProximitySensor", mockproximitysensor::id);
    }


    QSensorBackend *createBackend(QSensor *sensor)
    {
        if (sensor->identifier() == mockaccelerometer::id) {
            return new mockaccelerometer(sensor);
        }

        if (sensor->identifier() == mockorientationsensor::id) {
            return new mockorientationsensor(sensor);
        }

        if (sensor->identifier() == mockirproximitysensor::id) {
            return  new mockirproximitysensor(sensor);
        }
        if (sensor->identifier() == mocktapsensor::id) {
             return  new mocktapsensor(sensor);
        }
        if (sensor->identifier() == mockproximitysensor::id) {
            return new mockproximitysensor(sensor);
        }

        qWarning() << "Can't create backend" << sensor->identifier();
        return 0;
    }
};

#endif
