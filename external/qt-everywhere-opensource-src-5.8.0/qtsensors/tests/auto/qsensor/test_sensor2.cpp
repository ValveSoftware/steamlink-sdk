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

#include "test_sensor2.h"
#include "test_sensor2_p.h"

#undef IMPLEMENT_READING
#undef IMPLEMENT_READING_D

#define IMPLEMENT_READING(classname)\
        IMPLEMENT_READING_D(classname, classname ## Private)

#define IMPLEMENT_READING_D(classname, pclassname)\
    classname::classname(QObject *parent)\
        : QSensorReading(parent, new pclassname)\
        , d(d_ptr())\
        {}\
    classname::~classname() {}\
    void classname::copyValuesFrom(QSensorReading *_other)\
    {\
        /* No need to verify types, only called by QSensorBackend */\
        classname *other = static_cast<classname *>(_other);\
        pclassname *my_ptr = static_cast<pclassname*>(d_ptr()->data());\
        pclassname *other_ptr = static_cast<pclassname*>(other->d_ptr()->data());\
        /* Do a direct copy of the private class */\
        *(my_ptr) = *(other_ptr);\
    }

IMPLEMENT_READING(TestSensor2Reading)

int TestSensor2Reading::test() const
{
    return d->test;
}

void TestSensor2Reading::setTest(int test)
{
    d->test = test;
}

// =====================================================================

char const * const TestSensor2::type("test sensor 2");

#include "moc_test_sensor2.cpp"
