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
#include "bbrotationsensor.h"

#include "bbguihelper.h"
#include "bbutil.h"

using namespace BbUtil;

BbRotationSensor::BbRotationSensor(QSensor *sensor)
    : BbSensorBackend<QRotationReading>(devicePath(), SENSOR_TYPE_ROTATION_MATRIX, sensor)
{
    setDescription(QLatin1String("Device rotation in degrees"));
}

QString BbRotationSensor::devicePath()
{
    return QLatin1String("/dev/sensor/rotMatrix");
}

void BbRotationSensor::additionalDeviceInit()
{
    addOutputRange(-180, 180, 0 /* ? */);
}

bool BbRotationSensor::addDefaultRange()
{
    // The range we get from the OS service is only -1 to 1, which are the values of the matrix.
    // We need the values of the axes in degrees.
    return false;
}

bool BbRotationSensor::updateReadingFromEvent(const sensor_event_t &event, QRotationReading *reading)
{
    // sensor_event_t has euler angles for a Z-Y'-X'' system, but the QtSensors API
    // uses Z-X'-Y''.
    // So extract the euler angles using the Z-X'-Y'' system from the matrix.
    float xRad, yRad, zRad;
    float mappedRotationMatrix[3*3];
    remapMatrix(event.rotation_matrix, mappedRotationMatrix);
    matrixToEulerZXY(mappedRotationMatrix, xRad, yRad, zRad);

    reading->setFromEuler(radiansToDegrees(xRad),
                          radiansToDegrees(yRad),
                          radiansToDegrees(zRad));

    return true;
}

qreal BbRotationSensor::convertValue(float bbValueInRad)
{
    return radiansToDegrees(bbValueInRad);
}
