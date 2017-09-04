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

#ifndef TEST_SENSOR_H
#define TEST_SENSOR_H

#include <qsensor.h>

class TestSensorReadingPrivate;

class TestSensorReading : public QSensorReading
{
    Q_OBJECT
    Q_PROPERTY(int test READ test)
    DECLARE_READING(TestSensorReading)
public:
    int test() const;
    void setTest(int test);
};

class TestSensorFilter : public QSensorFilter
{
public:
    virtual bool filter(TestSensorReading *reading) = 0;
private:
    bool filter(QSensorReading *reading) { return filter(static_cast<TestSensorReading*>(reading)); }
};

class TestSensor : public QSensor
{
    Q_OBJECT
public:
    explicit TestSensor(QObject *parent = 0)
        : QSensor(TestSensor::type, parent)
        , sensorsChangedEmitted(0)
    {
        connect(this, SIGNAL(availableSensorsChanged()), this, SLOT(s_availableSensorsChanged()));
    }
    virtual ~TestSensor() {}
    TestSensorReading *reading() const { return static_cast<TestSensorReading*>(QSensor::reading()); }
    static const char *type;

    // used by the testSensorsChangedSignal test function
    int sensorsChangedEmitted;
private slots:
    void s_availableSensorsChanged()
    {
        sensorsChangedEmitted++;
    }
};

#endif
