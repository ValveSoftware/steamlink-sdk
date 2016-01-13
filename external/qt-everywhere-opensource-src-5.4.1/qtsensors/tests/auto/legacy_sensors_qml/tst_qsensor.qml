/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtSensors 5.0 as Sensors
import TestHelper 1.0

TestCase {
    id: test
    name: "tst_qsensor"

    TestHelper {
        id: helper
    }

    Component {
        id: cTestSensor
        TestSensor{}
    }

    function initTestCase() {}

    function cleanupTestCase() {}

    function init() {}

    function cleanup() {}

    function sortlist(list)
    {
        var array = new Array();
        for (var ii in list)
            array.push(list[ii]);
        array.sort();
        return array;
    }

    function test_001_TypeRegistered()
    {
        var expected = [ "test sensor", "test sensor 2" ];
        var actual = Sensors.sensorTypes();
        // FIXME .sort() is broken!
        actual = sortlist(actual); // The actual list is not in a defined order
        compare(actual, expected);
    }

    function test_002_SensorRegistered()
    {
        var expected = [ "test sensor 2", "test sensor 3", "test sensor impl" ];
        var actual = Sensors.sensorsForType("test sensor");
        // FIXME .sort() is broken!
        actual = sortlist(actual); // The actual list is not in a defined order
        compare(actual, expected);
    }

    function test_003_SensorDefault()
    {
        var expected = "test sensor impl";
        var actual = Sensors.defaultSensorForType("test sensor");
        compare(actual, expected);
    }

    function test_004_NoSensorsForType()
    {
        var expected = [];
        var actual = Sensors.sensorsForType("bogus type");
        compare(actual, expected);
    }

    function test_005_NoDefaultForType()
    {
        var expected = "";
        var actual = Sensors.defaultSensorForType("bogus type");
        compare(actual, expected);
    }

    function test_006_Creation()
    {
        var sensor = cTestSensor.createObject();
        compare(sensor.connectedToBackend, true);
        var expected = "test sensor impl";
        var actual = sensor.sensorid;
        compare(actual, expected);
        sensor.destroy();
    }

    function test_008_Timestamp()
    {
        var sensor = cTestSensor.createObject();
        compare(sensor.connectedToBackend, true);
        var timestamp = sensor.reading.timestamp;
        compare(timestamp, 0);
        sensor.doThis = "setOne";
        sensor.start();
        timestamp = sensor.reading.timestamp;
        compare(timestamp, 1);
        sensor.destroy();
    }

    function test_009_Start()
    {
        var sensor = cTestSensor.createObject();
        sensor.start();
        compare(sensor.active, true);
        sensor.start();
        compare(sensor.active, true);
        sensor.destroy();
    }

    function test_010_Stop()
    {
        var sensor = cTestSensor.createObject();
        sensor.stop();
        compare(sensor.active, false);
        sensor.start();
        compare(sensor.active, true);
        sensor.stop();
        compare(sensor.active, false);
        sensor.destroy();
    }

    function test_011_Start2()
    {
        var sensor = cTestSensor.createObject();

        sensor.doThis = "stop";
        sensor.start();
        compare(sensor.active, false);
        sensor.stop();

        sensor.doThis = "error";
        sensor.start();
        compare(sensor.error, 1);
        // Yes, this is non-intuitive but the sensor
        // decides if an error is fatal or not.
        // In this case our test sensor is reporting a
        // non-fatal error so the sensor will start.
        compare(sensor.active, true);
        sensor.stop();

        sensor.doThis = "setOne";
        sensor.start();
        compare(sensor.reading.timestamp, 1);
        sensor.stop();

        sensor.doThis = "setTwo";
        sensor.start();
        compare(sensor.reading.timestamp, 2);
        sensor.stop();
        sensor.destroy();
    }

    function test_012_SetBadDataRate()
    {
        var sensor = cTestSensor.createObject();

        ignoreWarning("setDataRate: 1 is not supported by the sensor. ");
        sensor.dataRate = 1;
        compare(sensor.dataRate, 0);

        ignoreWarning("setDataRate: 1000 is not supported by the sensor. ");
        sensor.dataRate = 1000;
        compare(sensor.dataRate, 0);
        sensor.destroy();
    }
}
