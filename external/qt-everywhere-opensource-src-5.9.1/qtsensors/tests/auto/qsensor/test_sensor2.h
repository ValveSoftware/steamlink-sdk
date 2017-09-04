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

#ifndef TEST_SENSOR2_H
#define TEST_SENSOR2_H

#include "qsensor.h"

#undef DECLARE_READING
#undef DECLARE_READING_D

template <typename T>
class qTypedWrapper
{
public:
    qTypedWrapper(QScopedPointer<QSensorReadingPrivate> *_ptr)
        : ptr(_ptr)
    {
    }

    T *operator->() const
    {
        return static_cast<T*>(ptr->data());
    }

private:
    QScopedPointer<QSensorReadingPrivate> *ptr;
};

#define DECLARE_READING(classname)\
        DECLARE_READING_D(classname, classname ## Private)

#define DECLARE_READING_D(classname, pclassname)\
    public:\
        classname(QObject *parent = 0);\
        virtual ~classname();\
        void copyValuesFrom(QSensorReading *other);\
    private:\
        qTypedWrapper<pclassname> d;

class TestSensor2ReadingPrivate;

class TestSensor2Reading : public QSensorReading
{
    Q_OBJECT
    Q_PROPERTY(int test READ test)
    DECLARE_READING(TestSensor2Reading)
public:
    int test() const;
    void setTest(int test);
};

class TestSensor2Filter : public QSensorFilter
{
public:
    virtual bool filter(TestSensor2Reading *reading) = 0;
private:
    bool filter(QSensorReading *reading) { return filter(static_cast<TestSensor2Reading*>(reading)); }
};

class TestSensor2 : public QSensor
{
    Q_OBJECT
public:
    explicit TestSensor2(QObject *parent = 0) : QSensor(TestSensor2::type, parent) {}
    virtual ~TestSensor2() {}
    TestSensor2Reading *reading() const { return static_cast<TestSensor2Reading*>(QSensor::reading()); }
    static char const * const type;
};

#endif
