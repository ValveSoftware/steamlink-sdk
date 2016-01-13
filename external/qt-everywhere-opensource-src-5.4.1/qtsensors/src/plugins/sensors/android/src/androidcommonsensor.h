/****************************************************************************
**
** Copyright (C) 2013 BogDan Vatra <bogdan@kde.org>
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

#ifndef ANDROIDCOMMONSENSOR_H
#define ANDROIDCOMMONSENSOR_H

#include <qsensorbackend.h>
#include <qsensor.h>
#include "androidjnisensors.h"

template <typename ReaderType>
class AndroidCommonSensor : public QSensorBackend, protected AndroidSensors::AndroidSensorsListenerInterface
{
public:
    AndroidCommonSensor(AndroidSensors::AndroidSensorType type, QSensor *sensor) : QSensorBackend(sensor)
    {
        setDescription(AndroidSensors::sensorDescription(type));
        setReading<ReaderType>(&m_reader);
        m_type = type;
        m_isStarted = false;
    }

    virtual ~AndroidCommonSensor()
    {
        if (m_isStarted)
            stop();
    }
    void start() Q_DECL_OVERRIDE
    {
        if (AndroidSensors::registerListener(m_type, this, sensor()->dataRate()))
            m_isStarted = true;
    }

    void stop() Q_DECL_OVERRIDE
    {
        if (m_isStarted) {
            m_isStarted = false;
            AndroidSensors::unregisterListener(m_type, this);
        }
    }

protected:
    ReaderType m_reader;
    AndroidSensors::AndroidSensorType m_type;

private:
    bool m_isStarted;
};

#endif // ANDROIDCOMMONSENSOR_H
