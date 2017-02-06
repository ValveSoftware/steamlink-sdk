/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bogdan@kde.org>
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
