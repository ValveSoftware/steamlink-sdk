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

#include "test_sensor2impl.h"
#include <qaccelerometer.h>
#include <QDebug>

char const * const testsensor2impl::id("test sensor 2 impl");

testsensor2impl::testsensor2impl(QSensor *sensor)
    : QSensorBackend(sensor)
{
    setReading<TestSensor2Reading>(&m_reading);
}

void testsensor2impl::start()
{
    QString doThis = sensor()->property("doThis").toString();
    if (doThis == "setOne") {
        m_reading.setTimestamp(1);
        m_reading.setTest(1);
        newReadingAvailable();
    } else {
        m_reading.setTimestamp(2);
        m_reading.setTest(2);
        newReadingAvailable();
    }
}

void testsensor2impl::stop()
{
}

