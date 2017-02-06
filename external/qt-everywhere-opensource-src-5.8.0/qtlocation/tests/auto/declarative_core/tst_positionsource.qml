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
import QtPositioning 5.2

TestCase {
    id: testCase

    name: "PositionSource"

    PositionSource { id: defaultSource }
    PositionSource
    {
        id: activeDefaultSource
        active: true
    }

    SignalSpy { id: defaultSourceSpy; target: defaultSource; signalName: "positionChanged" }

    function test_activeDefaultSource() {
        wait(0);
        verify(activeDefaultSource.name !== "");
        compare(activeDefaultSource.active, true);
    }

    function test_invalidSource() {
        activeDefaultSource.name = "invalid_positioning_source";
        verify(!activeDefaultSource.active);
        verify(!activeDefaultSource.valid);
    }

    function test_defaults() {
        // at least the test.source plugin should be available
        verify(defaultSource.name != "");
        compare(defaultSource.active, false);
    }

    function test_inactive() {
        defaultSourceSpy.clear();
        compare(defaultSourceSpy.count, 0);
        wait(1000);
        compare(defaultSourceSpy.count, 0);
    }

    PositionSource { id: testSetSource; name: "nonexistent bogus plugin" }
    SignalSpy { id: testingSourcePluginSpy; target: testSetSource; signalName: "nameChanged" }

    function test_setplugin() {
        testingSourcePluginSpy.clear();

        // On construction, if the provided source name is invalid, the default source will be
        // used. Test that the source is valid as expected.
        verify(testSetSource.name !== "");
        //we don't really know what the default source is named.
        //It may not be "test.source"
        var defaultSourceName = testSetSource.name;
        verify(testSetSource.valid);

        // Test that setting name to "" will still use the default.
        testSetSource.name = "";
        compare(testingSourcePluginSpy.count, 0);
        compare(testSetSource.name, defaultSourceName);
        verify(testSetSource.valid);

        testSetSource.name = "test.source";
        if (defaultSourceName === "test.source")
            compare(testingSourcePluginSpy.count, 0);
        compare(testSetSource.name, "test.source");
        verify(testSetSource.valid);
        testingSourcePluginSpy.clear();

        testSetSource.name = "bogus";
        compare(testingSourcePluginSpy.count, 1);
        verify(!testSetSource.valid);
    }

    PositionSource { id: testingSource; name: "test.source"; updateInterval: 1000 }
    SignalSpy { id: updateSpy; target: testingSource; signalName: "positionChanged" }
    SignalSpy { id: directionValidSpy; target: testingSource.position; signalName: "directionValidChanged" }
    SignalSpy { id: directionSpy; target: testingSource.position; signalName: "directionChanged" }

    function test_updateInterval() {
        testingSource.updateInterval = 1000;
        compare(testingSource.updateInterval, 1000);
        testingSource.updateInterval = 1200;
        compare(testingSource.updateInterval, 1200);
        testingSource.updateInterval = 800;
        compare(testingSource.updateInterval, 1000);
    }

    function test_preferredPositioningMethods() {
        testingSource.preferredPositioningMethods = PositionSource.AllPositioningMethods;
        compare(testingSource.preferredPositioningMethods, PositionSource.AllPositioningMethods);
        testingSource.preferredPositioningMethods = PositionSource.SatellitePositioningMethods;
        compare(testingSource.preferredPositioningMethods, PositionSource.SatellitePositioningMethods);
        testingSource.preferredPositioningMethods = PositionSource.NonSatellitePositioningMethods;
        compare(testingSource.preferredPositioningMethods, PositionSource.NonSatellitePositioningMethods);
    }

    function test_updates() {
        updateSpy.clear();

        compare(directionValidSpy.count, 0)
        compare(directionSpy.count, 0)

        testingSource.active = true;

        tryCompare(updateSpy, "count", 1, 1500);
        compare(testingSource.position.coordinate.longitude, 0.1);
        compare(testingSource.position.coordinate.latitude, 0.1);
        compare(directionValidSpy.count, 1)
        compare(directionSpy.count, 1)
        fuzzyCompare(testingSource.position.direction, 45, 0.1)
        verify(!testingSource.position.speedValid)
        verify(isNaN(testingSource.position.speed))

        tryCompare(updateSpy, "count", 2, 1500);
        compare(testingSource.position.coordinate.longitude, 0.2);
        compare(testingSource.position.coordinate.latitude, 0.2);
        compare(directionValidSpy.count, 1)
        compare(directionSpy.count, 2)
        fuzzyCompare(testingSource.position.direction, 45, 0.1)
        verify(testingSource.position.speedValid)
        verify(testingSource.position.speed > 10000)

        testingSource.active = false;
        wait(2500);
        compare(updateSpy.count, 2);
        compare(testingSource.position.coordinate.longitude, 0.2);
        compare(testingSource.position.coordinate.latitude, 0.2);
        compare(directionValidSpy.count, 1)
        compare(directionSpy.count, 2)
        fuzzyCompare(testingSource.position.direction, 45, 0.1)
        verify(testingSource.position.speedValid)
        verify(testingSource.position.speed > 10000)
    }
}
