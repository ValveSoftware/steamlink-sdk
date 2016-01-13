/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
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
#include "bbgyroscope.h"

#include "bbutil.h"
#include <QtCore/qmath.h>

using namespace BbUtil;

BbGyroscope::BbGyroscope(QSensor *sensor)
    : BbSensorBackend<QGyroscopeReading>(devicePath(), SENSOR_TYPE_GYROSCOPE, sensor)
{
    setDescription(QLatin1String("Angular velocities around x, y, and z axis in degrees per second"));
}

bool BbGyroscope::updateReadingFromEvent(const sensor_event_t &event, QGyroscopeReading *reading)
{
    float x = radiansToDegrees(event.motion.dsp.x);
    float y = radiansToDegrees(event.motion.dsp.y);
    float z = radiansToDegrees(event.motion.dsp.z);
    remapAxes(&x, &y, &z);
    reading->setX(x);
    reading->setY(y);
    reading->setZ(z);

    return true;
}

QString BbGyroscope::devicePath()
{
    return QLatin1String("/dev/sensor/gyro");
}

qreal BbGyroscope::convertValue(float valueInRadiansPerSecond)
{
    return radiansToDegrees(valueInRadiansPerSecond);
}
