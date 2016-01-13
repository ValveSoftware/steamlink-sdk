/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
