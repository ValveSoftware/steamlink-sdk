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
#include "bborientationsensor.h"

BbOrientationSensor::BbOrientationSensor(QSensor *sensor)
    : BbSensorBackend<QOrientationReading>(devicePath(), SENSOR_TYPE_ORIENTATION, sensor)
{
    setDescription(QLatin1String("Device orientation"));

    // Orientation rarely changes, so enable skipping of duplicates by default
    sensor->setSkipDuplicates(true);
}

QString BbOrientationSensor::devicePath()
{
    return QLatin1String("/dev/sensor/orientation");
}

void BbOrientationSensor::additionalDeviceInit()
{
    // When querying the OS service for the range, it gives us the angles, which we don't need.
    // So set the possible enum values of QOrientationReading::Orientation as the output range here.
    // By returning false in addDefaultRange(), we skip setting the range from the OS service in the
    // base class.
    addOutputRange(0, 6, 1);
}

bool BbOrientationSensor::addDefaultRange()
{
    return false;
}

bool BbOrientationSensor::updateReadingFromEvent(const sensor_event_t &event, QOrientationReading *reading)
{
    QOrientationReading::Orientation qtOrientation = QOrientationReading::Undefined;
    const QByteArray face(event.orientation.face);
    if (face == "FACE_UP") qtOrientation = QOrientationReading::FaceUp;
    else if (face == "TOP_UP") qtOrientation = QOrientationReading::TopUp;
    else if (face == "RIGHT_UP") qtOrientation = QOrientationReading::RightUp;
    else if (face == "LEFT_UP") qtOrientation = QOrientationReading::LeftUp;
    else if (face == "BOTTOM_UP") qtOrientation = QOrientationReading::TopDown;
    else if (face == "FACE_DOWN") qtOrientation = QOrientationReading::FaceDown;

    reading->setOrientation(qtOrientation);
    return true;
}
