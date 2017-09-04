/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import QtPositioning 5.2

Item {
    Plugin { id: testPlugin1; name: "qmlgeo.test.plugin"; allowExperimental: true}
    Plugin { id: errorPlugin; name: "qmlgeo.test.plugin"; allowExperimental: true
        parameters: [
            PluginParameter { name: "error"; value: "1"},
            PluginParameter { name: "errorString"; value: "This error was expected. No worries !"}
        ]
    }


    property variant coordinate1: QtPositioning.coordinate(51, 41)
    property variant coordinate2: QtPositioning.coordinate(52, 42)
    property variant coordinate3: QtPositioning.coordinate(53, 43)
    property variant emptyCoordinate: QtPositioning.coordinate()

    property variant boundingBox1: QtPositioning.rectangle(coordinate1, coordinate2)
    property variant boundingBox2: QtPositioning.rectangle(coordinate1, coordinate3)
    property variant boundingCircle1: QtPositioning.circle(coordinate1, 100)
    property variant boundingCircle2: QtPositioning.circle(coordinate2, 100)

    property variant emptyBox: QtPositioning.rectangle()

    GeocodeModel {id: emptyModel}

    Address {id: emptyAddress}
    SignalSpy {id: querySpy; target: emptyModel; signalName: "queryChanged"}
    SignalSpy {id: autoUpdateSpy; target: emptyModel; signalName: "autoUpdateChanged"}
    SignalSpy {id: pluginSpy; target: emptyModel ; signalName: "pluginChanged"}
    SignalSpy {id: boundsSpy; target: emptyModel; signalName: "boundsChanged"}
    SignalSpy {id: limitSpy; target: emptyModel; signalName: "limitChanged"}
    SignalSpy {id: offsetSpy; target: emptyModel; signalName: "offsetChanged"}

    TestCase {
        id: testCase1
        name: "GeocodeModel"
        function test_model_defaults_and_setters() {
            // Query: address
            compare (querySpy.count, 0)
            emptyModel.query = address1
            compare (querySpy.count, 1)
            compare (emptyModel.query.street, address1.street)
            emptyModel.query = address1
            compare (querySpy.count, 1)
            compare (emptyModel.query.street, address1.street)
            // Query: coordinate
            emptyModel.query = coordinate1
            compare (querySpy.count, 2)
            compare (emptyModel.query.latitude, coordinate1.latitude)
            emptyModel.query = coordinate1
            compare (querySpy.count, 2)
            compare (emptyModel.query.latitude, coordinate1.latitude)
            // Query: string
            emptyModel.query = "Kuortane, Finland"
            compare (querySpy.count, 3)
            compare (emptyModel.query, "Kuortane, Finland")
            emptyModel.query = "Kuortane, Finland"
            compare (querySpy.count, 3)
            compare (emptyModel.query, "Kuortane, Finland")

            // limit and offset
            compare (limitSpy.count, 0)
            compare (offsetSpy.count, 0)
            compare(emptyModel.limit, -1)
            compare(emptyModel.offset, 0)
            emptyModel.limit = 2
            compare (limitSpy.count, 1)
            emptyModel.limit = 2
            compare (limitSpy.count, 1)
            emptyModel.offset = 10
            compare (offsetSpy.count, 1)
            emptyModel.offset = 10
            compare (offsetSpy.count, 1)

            // bounding box
            compare(boundsSpy.count, 0)
            emptyModel.bounds = boundingBox1
            compare(boundsSpy.count, 1)
            compare(emptyModel.bounds.topLeft.latitude, boundingBox1.topLeft.latitude)
            compare(emptyModel.bounds.bottomRight.longitude, boundingBox1.bottomRight.longitude)
            emptyModel.bounds = boundingBox1
            compare(boundsSpy.count, 1)
            compare(emptyModel.bounds.topLeft.latitude, boundingBox1.topLeft.latitude)
            compare(emptyModel.bounds.bottomRight.longitude, boundingBox1.bottomRight.longitude)
            emptyModel.bounds = boundingBox2
            compare(boundsSpy.count, 2)
            compare(emptyModel.bounds.topLeft.latitude, boundingBox2.topLeft.latitude)
            compare(emptyModel.bounds.bottomRight.longitude, boundingBox2.bottomRight.longitude)
            emptyModel.bounds = QtPositioning.rectangle();
            compare(boundsSpy.count, 3)


            // bounding circle
            boundsSpy.clear()
            emptyModel.bounds = boundingCircle1
            compare(boundsSpy.count, 1)
            compare(emptyModel.bounds.center.latitude, coordinate1.latitude)
            emptyModel.bounds = boundingCircle1
            compare(boundsSpy.count, 1)
            compare(emptyModel.bounds.center.latitude, coordinate1.latitude)
            emptyModel.bounds = boundingCircle2
            compare(boundsSpy.count, 2)
            compare(emptyModel.bounds.center.latitude, coordinate2.latitude)
            var dynamicCircle = QtPositioning.circle(QtPositioning.coordinate(8, 9));
            emptyModel.bounds = dynamicCircle
            compare(boundsSpy.count, 3)
            compare(emptyModel.bounds.center.latitude, dynamicCircle.center.latitude)

            // status
            compare (emptyModel.status, GeocodeModel.Null)

            // error
            compare (emptyModel.errorString, "")
            compare (emptyModel.error, GeocodeModel.NoError)

            // count
            compare( emptyModel.count, 0)

            // auto update
            compare (autoUpdateSpy.count, 0)
            compare (emptyModel.autoUpdate, false)
            emptyModel.autoUpdate = true
            compare (emptyModel.autoUpdate, true)
            compare (autoUpdateSpy.count, 1)
            emptyModel.autoUpdate = true
            compare (emptyModel.autoUpdate, true)
            compare (autoUpdateSpy.count, 1)

            // mustn't crash even we don't have plugin
            emptyModel.update()

            // Plugin
            compare(pluginSpy.count, 0)
            emptyModel.plugin = testPlugin1
            compare(pluginSpy.count, 1)
            compare(emptyModel.plugin, testPlugin1)
            emptyModel.plugin = testPlugin1
            compare(pluginSpy.count, 1)
            emptyModel.plugin = errorPlugin
            compare(pluginSpy.count, 2)
        }
        // Test that model acts gracefully when plugin is not set or is invalid
        // (does not support geocoding)
        GeocodeModel {id: errorModel; plugin: errorPlugin}
        GeocodeModel {id: errorModelNoPlugin}
        SignalSpy {id: countInvalidSpy; target: errorModel; signalName: "countChanged"}
        SignalSpy {id: errorSpy; target: errorModel; signalName: "errorChanged"}
        function test_error_plugin() {
            // test plugin not set
            compare(errorModelNoPlugin.error,GeocodeModel.NoError)
            errorModelNoPlugin.update()
            compare(errorModelNoPlugin.error,GeocodeModel.EngineNotSetError)
            console.log(errorModelNoPlugin.errorString)

            //plugin set but otherwise not offering anything
            compare(errorModel.error,GeocodeModel.EngineNotSetError)
            compare(errorModel.errorString,"This error was expected. No worries !")
            errorSpy.clear()
            errorModel.update()
            compare(errorModel.error,GeocodeModel.EngineNotSetError)
            compare(errorModel.errorString,qsTr("Cannot geocode, geocode manager not set."))
            compare(errorSpy.count, 1)
            errorSpy.clear()
            errorModel.cancel()
            compare(errorModel.error,GeocodeModel.NoError)
            compare(errorModel.errorString,"")
            compare(errorSpy.count, 1)
            errorSpy.clear()
            errorModel.reset()
            compare(errorModel.error,GeocodeModel.NoError)
            compare(errorModel.errorString,"")
            compare(errorSpy.count, 0)
            errorSpy.clear()
            errorModel.update()
            compare(errorModel.error,GeocodeModel.EngineNotSetError)
            compare(errorModel.errorString,qsTr("Cannot geocode, geocode manager not set."))
            compare(errorSpy.count, 1)
            errorSpy.clear()
            var location = errorModel.get(-1)
            compare(location, null)
        }

    }
    Address {id: address1; street: "wellknown street"; city: "expected city"; county: "2"}
    Address {id: errorAddress1; street: "error"; county: "2"} // street is the error reason

    property variant rcoordinate1: QtPositioning.coordinate(51, 2)
    property variant errorCoordinate1: QtPositioning.coordinate(73, 2)  // (latiude mod 70) is the error code
    property variant slackCoordinate1: QtPositioning.coordinate(60, 3)
    Address {id: slackAddress1; street: "Slacker st"; city: "Lazy town"; county: "4"}

    property variant automaticCoordinate1: QtPositioning.coordinate(60, 3)
    Address {id: automaticAddress1; street: "Auto st"; city: "Detroit"; county: "4"}

    Plugin {
        id: testPlugin2;
        name: "qmlgeo.test.plugin"
        allowExperimental: true
        parameters: [
            // Parms to guide the test plugin
            PluginParameter { name: "supported"; value: true},
            PluginParameter { name: "finishRequestImmediately"; value: true},
            PluginParameter { name: "validateWellKnownValues"; value: true}
        ]
    }

    Plugin {
        id: immediatePlugin;
        name: "qmlgeo.test.plugin"
        allowExperimental: true
        parameters: [
            // Parms to guide the test plugin
            PluginParameter { name: "supported"; value: true},
            PluginParameter { name: "finishRequestImmediately"; value: true},
            PluginParameter { name: "validateWellKnownValues"; value: false}
        ]
    }

    Plugin {
        id: slackPlugin;
        allowExperimental: true
        name: "qmlgeo.test.plugin"
        parameters: [
            // Parms to guide the test plugin
            PluginParameter { name: "supported"; value: true},
            PluginParameter { name: "finishRequestImmediately"; value: false},
            PluginParameter { name: "validateWellKnownValues"; value: false}
        ]
    }

    Plugin {
        id: autoPlugin;
        allowExperimental: true
        name: "qmlgeo.test.plugin"
        parameters: [
            // Parms to guide the test plugin
            PluginParameter { name: "supported"; value: true},
            PluginParameter { name: "finishRequestImmediately"; value: false},
            PluginParameter { name: "validateWellKnownValues"; value: false}
        ]
    }

    GeocodeModel {id: testModel; plugin: testPlugin2}
    SignalSpy {id: locationsSpy; target: testModel; signalName: "locationsChanged"}
    SignalSpy {id: countSpy; target: testModel; signalName: "countChanged"}
    SignalSpy {id: testQuerySpy; target: testModel; signalName: "queryChanged"}
    SignalSpy {id: testStatusSpy; target: testModel; signalName: "statusChanged"}

    GeocodeModel {id: slackModel; plugin: slackPlugin; }
    SignalSpy {id: locationsSlackSpy; target: slackModel; signalName: "locationsChanged"}
    SignalSpy {id: countSlackSpy; target: slackModel; signalName: "countChanged"}
    SignalSpy {id: querySlackSpy; target: slackModel; signalName: "queryChanged"}
    SignalSpy {id: errorStringSlackSpy; target: slackModel; signalName: "errorChanged"}
    SignalSpy {id: errorSlackSpy; target: slackModel; signalName: "errorChanged"}
    SignalSpy {id: pluginSlackSpy; target: slackModel; signalName: "pluginChanged"}

    GeocodeModel {id: immediateModel; plugin: immediatePlugin}
    SignalSpy {id: locationsImmediateSpy; target: immediateModel; signalName: "locationsChanged"}
    SignalSpy {id: countImmediateSpy; target: immediateModel; signalName: "countChanged"}
    SignalSpy {id: queryImmediateSpy; target: immediateModel; signalName: "queryChanged"}
    SignalSpy {id: statusImmediateSpy; target: immediateModel; signalName: "statusChanged"}
    SignalSpy {id: errorStringImmediateSpy; target: immediateModel; signalName: "errorChanged"}
    SignalSpy {id: errorImmediateSpy; target: immediateModel; signalName: "errorChanged"}

    GeocodeModel {id: automaticModel; plugin: autoPlugin; query: automaticAddress1; autoUpdate: true}
    SignalSpy {id: automaticLocationsSpy; target: automaticModel; signalName: "locationsChanged"}

    TestCase {
        name: "GeocodeModelGeocoding"
        function clear_slack_model() {
            slackModel.reset()
            locationsSlackSpy.clear()
            countSlackSpy.clear()
            querySlackSpy.clear()
            errorStringSlackSpy.clear()
            errorSlackSpy.clear()
            slackModel.limit = -1
            slackModel.offset = 0
        }
        function clear_immediate_model() {
            immediateModel.reset()
            locationsImmediateSpy.clear()
            countImmediateSpy.clear()
            queryImmediateSpy.clear()
            errorStringImmediateSpy.clear()
            errorImmediateSpy.clear()
            statusImmediateSpy.clear()
            immediateModel.limit = -1
            immediateModel.offset = 0
        }
        function test_reset() {
            clear_immediate_model();
            immediateModel.query = errorAddress1
            immediateModel.update()
            compare (immediateModel.errorString, errorAddress1.street)
            compare (immediateModel.error, GeocodeModel.CommunicationError)
            compare (immediateModel.count, 0)
            compare (statusImmediateSpy.count, 2)
            compare (immediateModel.status, GeocodeModel.Error)
            immediateModel.reset()
            compare (immediateModel.errorString, "")
            compare (immediateModel.error, GeocodeModel.NoError)
            compare (immediateModel.status, GeocodeModel.Null)
            // Check that ongoing req is aborted
            clear_slack_model()
            slackModel.query = slackAddress1
            slackAddress1.county = "5"
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            compare (locationsSlackSpy.count, 0)
            compare (slackModel.count, 0)
            slackModel.reset()
            tryCompare(countSlackSpy, "count", 0)
            compare (locationsSlackSpy.count, 0)
            compare (slackModel.count, 0)
            // Check that results are cleared
            slackModel.update()
            tryCompare(slackModel, "count", 5)
            slackModel.reset()
            compare (slackModel.count, 0)
            // Check that changing plugin resets any ongoing requests
            clear_slack_model()
            slackModel.query = slackAddress1
            slackAddress1.county = "7"
            compare (pluginSlackSpy.count, 0)
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            slackModel.plugin = errorPlugin
            tryCompare(countSlackSpy, "count", 0)
            compare (pluginSlackSpy.count, 1)
            // switch back and check that works
            slackModel.plugin = slackPlugin
            compare (pluginSlackSpy.count, 2)
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            tryCompare(countSlackSpy, "count", 1)
        }
        function test_error_geocode() {
            // basic immediate geocode error
            clear_immediate_model()
            immediateModel.query = errorAddress1
            immediateModel.update()
            compare (errorStringImmediateSpy.count, 1)
            compare (immediateModel.errorString, errorAddress1.street)
            compare (immediateModel.error, GeocodeModel.CommunicationError) // county of the address (2)
            compare (immediateModel.count, 0)
            compare (statusImmediateSpy.count, 2)
            compare (immediateModel.status, GeocodeModel.Error)
            // basic delayed geocode error
            clear_slack_model()
            slackModel.query = errorAddress1
            errorAddress1.street = "error code 2"
            slackModel.update()
            compare (errorStringSlackSpy.count, 0)
            compare (errorSlackSpy.count, 0)
            tryCompare (errorStringSlackSpy, "count", 1)
            tryCompare (errorSlackSpy, "count", 1)
            compare (slackModel.errorString, errorAddress1.street)
            compare (slackModel.error, GeocodeModel.CommunicationError)
            compare (slackModel.count, 0)
            // Check that we recover
            slackModel.query = address1
            slackModel.update()
            tryCompare(countSlackSpy, "count", 1)
            compare (slackModel.count, 2)
            compare (errorStringSlackSpy.count, 2)
            compare (errorSlackSpy.count, 2)
            compare (slackModel.errorString, "")
            compare (slackModel.error, GeocodeModel.NoError)
        }

        function test_error_reverse_geocode() {
            // basic immediate geocode error
            clear_immediate_model()
            immediateModel.query = errorCoordinate1
            immediateModel.update()
            if (immediateModel.errorString != "")
                compare (errorStringImmediateSpy.count, 1) // the previous error is cleared upon update()
            else
                compare (errorImmediateSpy.count, 1)
            compare (immediateModel.errorString, "error")
            compare (immediateModel.error, GeocodeModel.ParseError)
            compare (immediateModel.count, 0)
            compare (statusImmediateSpy.count, 2)
            compare (immediateModel.status, GeocodeModel.Error)
            // basic delayed geocode error
            clear_slack_model()
            slackModel.query = errorCoordinate1
            slackModel.update()
            compare (errorStringSlackSpy.count, 0)
            compare (errorSlackSpy.count, 0)
            if (slackModel.errorString != "")
                tryCompare (errorStringSlackSpy, "count", 2)
            else
                tryCompare (errorStringSlackSpy, "count", 1)
            compare (slackModel.errorString, "error")
            compare (slackModel.error, GeocodeModel.ParseError)
            compare (slackModel.count, 0)
            // Check that we recover
            slackModel.query = rcoordinate1
            slackModel.update()
            tryCompare(countSlackSpy, "count", 1)
            compare (slackModel.count, 2)
            compare (errorStringSlackSpy.count, 2)
            compare (errorSlackSpy.count, 2)
            compare (slackModel.errorString, "")
            compare (slackModel.error, GeocodeModel.NoError)
        }
        function test_address_geocode() {
            testQuerySpy.clear()
            locationsSpy.clear()
            testStatusSpy.clear()
            testModel.reset()
            countSpy.clear()
            compare (locationsSpy.count, 0)
            compare (testModel.errorString, "")
            compare (testModel.error, GeocodeModel.NoError)
            compare (testModel.count, 0)
            testModel.query = address1
            compare (testQuerySpy.count, 1)
            testModel.update()
            tryCompare (locationsSpy, "count", 1) // 5 sec
            compare (testModel.errorString, "")
            compare (testModel.error, GeocodeModel.NoError)
            compare (testModel.count, 2)
            compare (testQuerySpy.count, 1)
            compare (testStatusSpy.count, 2)
            compare (testModel.status, GeocodeModel.Ready)
            compare (testModel.get(0).address.street, "wellknown street")
            compare (testModel.get(0).address.city, "expected city")
        }

        function test_freetext_geocode() {
            testQuerySpy.clear()
            locationsSpy.clear()
            testStatusSpy.clear()
            testModel.reset()
            countSpy.clear()
            compare (locationsSpy.count, 0)
            compare (testModel.errorString, "")
            compare (testModel.error, GeocodeModel.NoError)
            compare (testModel.count, 0)
            testModel.limit = 5  // number of places echoed back
            testModel.offset = 10 // 'county' set in the places
            // Test successful case
            testModel.query = "Freetext geocode"
            compare(testQuerySpy.count, 1)
            testModel.update();
            tryCompare (locationsSpy, "count", 1) // 5 sec
            tryCompare(countSpy, "count", 1)
            tryCompare(testModel, "count", 5)
            compare(testModel.get(0).address.county, "10")
            // Test error case
            testModel.query = "2" // tells plugin to echo error '2'
            compare(testQuerySpy.count, 2)
            testModel.update();
            tryCompare (locationsSpy, "count", 2) // 5 sec
            tryCompare(countSpy, "count", 2)
            tryCompare(testModel, "count", 0)
            compare(testModel.errorString, "2")
            compare (testModel.error, GeocodeModel.CommunicationError)
            testModel.reset()
            tryCompare(countSpy, "count", 2)
            compare (testModel.count, 0)
        }

        function test_delayed_freetext_geocode() {
            clear_slack_model()
            slackModel.limit = 5  // number of places echoed back
            slackModel.offset = 10 // 'county' set in the places
            // Basic successful case
            slackModel.query = "freetext geocode"
            compare (querySlackSpy.count, 1)
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            compare (locationsSlackSpy.count, 0)
            compare (slackModel.count, 0)
            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare (slackModel.count, 5)
            compare (locationsSlackSpy.count, 1)
            // Frequent updates, previous requests are aborted
            slackModel.reset()
            locationsSlackSpy.clear()
            countSlackSpy.clear()
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare (locationsSlackSpy.count, 1)
            compare(slackModel.count, 5) // limit
        }

        function test_geocode_auto_updates() {
            compare (automaticModel.count, 4) // should be something already
            compare (automaticLocationsSpy.count, 1)
            // change query and its contents and verify that autoupdate occurs
            automaticAddress1.county = 6
            tryCompare(automaticLocationsSpy, "count", 2)
            compare (automaticModel.count, 6)
            automaticAddress1.street = "The Avenue"
            tryCompare(automaticLocationsSpy, "count", 3)
            compare (automaticModel.count, 6)
            automaticModel.query = automaticCoordinate1
            tryCompare(automaticLocationsSpy, "count", 4)
            compare (automaticModel.count, 3)
        }

        function test_delayed_geocode() {
            // basic delayed response
            slackModel.reset()
            querySlackSpy.clear()
            countSlackSpy.clear()
            locationsSlackSpy.clear()
            slackModel.query = slackAddress1
            slackAddress1.county = "7"
            compare (querySlackSpy.count, 1)
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            compare (locationsSlackSpy.count, 0)
            compare (slackModel.count, 0)
            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare (locationsSlackSpy.count, 1)
            compare (slackModel.count, 7) //  slackAddress1.county)
            // Frequent updates, previous requests are aborted
            slackModel.reset()
            locationsSlackSpy.clear()
            countSlackSpy.clear()
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare (locationsSlackSpy.count, 1)
            compare(slackModel.count, 7) // slackAddress1.county
        }
        function test_reverse_geocode() {
            testModel.reset()
            testQuerySpy.clear()
            locationsSpy.clear()
            testStatusSpy.clear()
            countSpy.clear()
            compare (testModel.errorString, "")
            compare (testModel.error, GeocodeModel.NoError)
            compare (testModel.count, 0)
            compare (testQuerySpy.count, 0)
            testModel.query = rcoordinate1
            compare (testQuerySpy.count, 1)
            testModel.update()
            tryCompare (locationsSpy, "count", 1) // 5 sec
            tryCompare(countSpy, "count", 1)
            compare (testModel.errorString, "")
            compare (testModel.error, GeocodeModel.NoError)
            compare (testModel.count, 2)
            testModel.reset()
            tryCompare(countSpy, "count", 2)
            compare (testModel.count, 0)
        }
        function test_delayed_reverse_geocode() {
            clear_slack_model()
            slackModel.query = slackCoordinate1
            compare (querySlackSpy.count, 1)
            slackModel.update()
            tryCompare(countSlackSpy, "count", 0)
            compare (locationsSlackSpy.count, 0)
            compare (slackModel.count, 0)

            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare (locationsSlackSpy.count, 1)
            compare (slackModel.count, 3) //  slackCoordinate1.longitude
            // Frequent updates, previous requests are aborted
            slackModel.reset()
            locationsSlackSpy.clear()
            countSlackSpy.clear()
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)
            slackModel.update()
            tryCompare(locationsSlackSpy, "count", 0)
            compare(countSlackSpy.count, 0)

            tryCompare(countSlackSpy, "count", 1); //waits up to 5s
            compare(locationsSlackSpy.count, 1)
            compare(slackModel.count, 3) // slackCoordinate1.longitude
        }
    }
}
