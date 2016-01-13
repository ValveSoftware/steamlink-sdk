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
#include "bbcompass.h"
#include "bbutil.h"

using namespace BbUtil;

BbCompass::BbCompass(QSensor *sensor)
#ifdef HAVE_COMPASS_SENSOR
    : BbSensorBackend<QCompassReading>(devicePath(), SENSOR_TYPE_COMPASS, sensor)
#else
    : BbSensorBackend<QCompassReading>(devicePath(), SENSOR_TYPE_ROTATION_MATRIX, sensor)
#endif
{
    setDescription(QLatin1String("Azimuth in degrees from magnetic north"));
}

bool BbCompass::updateReadingFromEvent(const sensor_event_t &event, QCompassReading *reading)
{
    float azimuth;
#ifdef HAVE_COMPASS_SENSOR
    azimuth = event.compass_s.azimuth;
#else
    float xRad, yRad, zRad;
    matrixToEulerZXY(event.rotation_matrix, xRad, yRad, zRad);
    azimuth = radiansToDegrees(zRad);
    if (azimuth < 0)
        azimuth = -azimuth;
    else
        azimuth = 360.0f - azimuth;
#endif

    if (isAutoAxisRemappingEnabled()) {
        azimuth += orientationForRemapping();
        if (azimuth >= 360.0f)
            azimuth -= 360.0f;
    }

    reading->setAzimuth(azimuth);


    switch (event.accuracy) {
    case SENSOR_ACCURACY_UNRELIABLE:
        reading->setCalibrationLevel(0.0f);
        break;
    case SENSOR_ACCURACY_LOW:
        reading->setCalibrationLevel(0.1f);
        break;

    // We determined that MEDIUM should map to 1.0, because existing code samples
    // show users should pop a calibration screen when seeing < 1.0. The MEDIUM accuracy
    // is actually good enough not to require calibration, so we don't want to make it seem
    // like it is required artificially.
    case SENSOR_ACCURACY_MEDIUM:
        reading->setCalibrationLevel(1.0f);
        break;
    case SENSOR_ACCURACY_HIGH:
        reading->setCalibrationLevel(1.0f);
        break;
    }

    return true;
}

QString BbCompass::devicePath()
{
#ifdef HAVE_COMPASS_SENSOR
    return QLatin1String("/dev/sensor/compass");
#else
    return QLatin1String("/dev/sensor/rotMatrix");
#endif
}
