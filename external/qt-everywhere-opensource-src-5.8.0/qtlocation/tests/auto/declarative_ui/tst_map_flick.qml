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

import QtQuick 2.5
import QtTest 1.0
import QtLocation 5.6
import QtPositioning 5.5

Item {
    // General-purpose elements for the test:
    id: page
    width: 100
    height: 100
    Plugin { id: testPlugin; name: "qmlgeo.test.plugin"; allowExperimental: true }

    property variant coordinate: QtPositioning.coordinate(10, 11)

    MouseArea {
        id: mouseAreaBottom
        anchors.fill: parent
        visible: false
    }

    Map {
        id: map
        plugin: testPlugin
        center: coordinate;
        zoomLevel: 9;
        anchors.fill: page
        x:0; y:0

        property real flickStartedLatitude
        property real flickStartedLongitude
        property bool disableOnPanStartedWithNoGesture: false
        property bool disableOnFlickStartedWithNoGesture: false
        property bool disableOnPanStartedWithDisabled: false
        property bool disableOnFlickStartedWithDisabled: false

        gesture.onPanStarted: {
            if (disableOnPanStartedWithNoGesture)
                map.gesture.acceptedGestures =  MapGestureArea.NoGesture
            if (disableOnPanStartedWithDisabled)
                map.gesture.enabled = false
        }
        gesture.onFlickStarted: {
            flickStartedLatitude = map.center.latitude
            flickStartedLatitude = map.center.longitude
            if (disableOnFlickStartedWithNoGesture)
                map.gesture.acceptedGestures =  MapGestureArea.NoGesture
            if (disableOnFlickStartedWithDisabled)
                map.gesture.enabled = false
        }
        MouseArea {
            id: mouseAreaTop
            anchors.fill: parent
            visible: false
        }
    }

    SignalSpy {id: centerSpy; target: map; signalName: 'centerChanged'}
    SignalSpy {id: panStartedSpy; target: map.gesture; signalName: 'panStarted'}
    SignalSpy {id: panFinishedSpy; target: map.gesture; signalName: 'panFinished'}
    SignalSpy {id: gestureEnabledSpy; target: map.gesture; signalName: 'enabledChanged'}
    SignalSpy {id: flickDecelerationSpy; target: map.gesture; signalName: 'flickDecelerationChanged'}
    SignalSpy {id: flickStartedSpy; target: map.gesture; signalName: 'flickStarted'}
    SignalSpy {id: flickFinishedSpy; target: map.gesture; signalName: 'flickFinished'}
    SignalSpy {id: mouseAreaTopSpy; target: mouseAreaTop; signalName: 'onPressed'}
    SignalSpy {id: mouseAreaBottomSpy; target: mouseAreaBottom; signalName: 'onPressed'}

    TestCase {
        when: windowShown
        name: "MapFlick"

        function init()
        {
            map.gesture.acceptedGestures = MapGestureArea.PanGesture | MapGestureArea.FlickGesture;
            map.gesture.enabled = true
            map.gesture.panEnabled = true
            map.gesture.flickDeceleration = 500
            map.zoomLevel = 0
            map.disableOnPanStartedWithNoGesture = false
            map.disableOnFlickStartedWithNoGesture = false
            map.disableOnPanStartedWithDisabled = false
            map.disableOnFlickStartedWithDisabled = false
            centerSpy.clear()
            gestureEnabledSpy.clear()
            flickDecelerationSpy.clear()
            panStartedSpy.clear()
            panFinishedSpy.clear()
            flickStartedSpy.clear()
            flickFinishedSpy.clear()
            mouseAreaTopSpy.clear()
            mouseAreaBottomSpy.clear()
            mouseAreaBottom.visible = false
            mouseAreaTop.visible = false
            compare(map.gesture.pinchActive, false)
            compare(map.gesture.panActive, false)
        }

        function initTestCase()
        {
            //check default values
            compare(map.gesture.enabled, true)
            map.gesture.enabled = false
            compare(gestureEnabledSpy.count, 1)
            compare(map.gesture.enabled, false)
            map.gesture.enabled = false
            compare(gestureEnabledSpy.count, 1)
            compare(map.gesture.enabled, false)
            map.gesture.enabled = true
            compare(gestureEnabledSpy.count, 2)
            compare(map.gesture.enabled, true)
            compare(map.gesture.pinchActive, false)
            compare(map.gesture.panActive, false)
            verify(map.gesture.acceptedGestures & MapGestureArea.PinchGesture)
            map.gesture.acceptedGestures = MapGestureArea.NoGesture
            compare(map.gesture.acceptedGestures, MapGestureArea.NoGesture)
            map.gesture.acceptedGestures = MapGestureArea.NoGesture
            compare(map.gesture.acceptedGestures, MapGestureArea.NoGesture)
            map.gesture.acceptedGestures = MapGestureArea.PinchGesture | MapGestureArea.PanGesture
            compare(map.gesture.acceptedGestures, MapGestureArea.PinchGesture | MapGestureArea.PanGesture)
            map.gesture.acceptedGestures = MapGestureArea.PanGesture
            compare(map.gesture.acceptedGestures, MapGestureArea.PanGesture)
            compare(map.gesture.flickDeceleration, 2500)
            map.gesture.flickDeceleration = 2600
            compare(flickDecelerationSpy.count, 1)
            compare(map.gesture.flickDeceleration, 2600)
            map.gesture.flickDeceleration = 2600
            compare(flickDecelerationSpy.count, 1)
            compare(map.gesture.flickDeceleration, 2600)
            map.gesture.flickDeceleration = 400 // too small
            compare(flickDecelerationSpy.count, 2)
            compare(map.gesture.flickDeceleration, 500) // clipped to min
            map.gesture.flickDeceleration = 11000 // too big
            compare(flickDecelerationSpy.count, 3)
            compare(map.gesture.flickDeceleration, 10000) // clipped to max
        }

        function flick_down()
        {
            map.center.latitude = 10
            map.center.longitude = 11
            mousePress(page, 0, 50)
            for (var i = 0; i < 50; i += 5) {
                wait(20)
                mouseMove(page, 0, (50 + i), 0, Qt.LeftButton);
            }
            mouseRelease(page, 0, 100)

            // order of signals is: flickStarted, either order: (flickEnded, movementEnded)
            verify(map.center.latitude > 10) // latitude increases we are going 'up/north' (moving mouse down)
            var moveLatitude = map.center.latitude // store lat and check that flick continues

            tryCompare(flickStartedSpy, "count", 1)
            tryCompare(panFinishedSpy, "count", 1)
            tryCompare(flickFinishedSpy, "count", 1)

            verify(map.center.latitude > moveLatitude)
            compare(map.center.longitude, 11) // should remain the same
        }

        function test_flick_down()
        {
            flick_down()
        }

        function test_flick_down_with_filtetring()
        {
            mouseAreaTop.visible = true
            mouseAreaBottom.visible = true
            flick_down()
            tryCompare(mouseAreaTopSpy, "count", 1)
            tryCompare(mouseAreaBottomSpy, "count",0)
        }

        function flick_up()
        {
            map.center.latitude = 70
            map.center.longitude = 11
            mousePress(page, 10, 95)
            for (var i = 45; i > 0; i -= 5) {
                wait(20)
                mouseMove(page, 10, (50 + i), 0, Qt.LeftButton);
            }
            mouseRelease(page, 10, 50)
            verify(map.center.latitude < 70)
            var moveLatitude = map.center.latitude // store lat and check that flick continues
            tryCompare(flickStartedSpy, "count", 1)
            tryCompare(panFinishedSpy, "count", 1)
            tryCompare(flickFinishedSpy, "count", 1)
            verify(map.center.latitude < moveLatitude)
            compare(map.center.longitude, 11) // should remain the same
        }

        function test_flick_up()
        {
            flick_up()
        }

        function test_flick_up_with_filtering()
        {
            mouseAreaTop.visible = true
            mouseAreaBottom.visible = true
            flick_up()
            tryCompare(mouseAreaTopSpy, "count", 1)
            tryCompare(mouseAreaBottomSpy, "count",0)
        }

        function test_flick_diagonal()
        {
            map.center.latitude = 50
            map.center.longitude = 50
            mousePress(page, 0, 0)
            for (var i = 0; i < 50; i += 5) {
                wait(20)
                mouseMove(page, i, i, 0, Qt.LeftButton);
            }
            mouseRelease(page, 50, 50)
            verify(map.center.latitude > 50)
            verify(map.center.longitude < 50)
            var moveLatitude = map.center.latitude
            var moveLongitude = map.center.longitude
            tryCompare(flickStartedSpy, "count", 1)
            tryCompare(panFinishedSpy, "count", 1)
            tryCompare(flickFinishedSpy, "count", 1)
            verify(map.center.latitude > moveLatitude)
            verify(map.center.longitude < moveLongitude)
        }

        function disabled_flicking()
        {
            map.center.latitude = 50
            map.center.longitude = 50
            mousePress(page, 0, 0)
            for (var i = 0; i < 50; i += 5) {
                wait(20)
                mouseMove(page, i, i, 0, Qt.LeftButton);
            }
            mouseRelease(page, 50, 50)
            compare(panStartedSpy.count, 0)
            compare(panFinishedSpy.count, 0)
            compare(flickStartedSpy.count, 0)
            compare(flickFinishedSpy.count, 0)
        }

        function test_disabled_flicking_with_nogesture()
        {
            map.gesture.acceptedGestures = MapGestureArea.NoGesture
        }

        function test_disabled_flicking_with_disabled()
        {
            map.gesture.enabled = false
            disabled_flicking()
        }

        function disable_onFlickStarted()
        {
            map.center.latitude = 50
            map.center.longitude = 50
            mousePress(page, 0, 0)
            for (var i = 0; i < 50; i += 5) {
                wait(20)
                mouseMove(page, i, i, 0, Qt.LeftButton);
            }
            mouseRelease(page, 50, 50)
            var latitude = map.center.latitude;
            var longitude = map.center.longitude
            tryCompare(panStartedSpy, "count", 1)
            tryCompare(flickStartedSpy, "count", 1)
            verify(map.center.latitude > 50)
            tryCompare(panFinishedSpy, "count", 1)
            tryCompare(flickFinishedSpy, "count", 1)
            // compare that flick was interrupted (less movement than without interrupting)
            compare(latitude, map.center.latitude)
            compare(longitude, map.center.longitude)
        }

        function test_disable_onFlickStarted_with_disabled()
        {
            map.disableOnFlickStartedWithDisabled = true
            disable_onFlickStarted()
        }

        function test_disable_onFlickStarted_with_nogesture()
        {
            map.disableOnFlickStartedWithNoGesture = true
            disable_onFlickStarted()
        }

        function disable_onPanStarted()
        {
            map.center.latitude = 50
            map.center.longitude = 50
            mousePress(page, 0, 0)
            for (var i = 0; i < 50; i += 5) {
                wait(20)
                mouseMove(page, i, i, 0, Qt.LeftButton);
            }
            mouseRelease(page, 50, 50)
            compare(map.center.latitude,50)
            compare(map.center.longitude,50)
            tryCompare(panFinishedSpy, "count", 1)
            // compare that flick was interrupted (less movement than without interrupting)
            compare(map.center.latitude,50)
            compare(map.center.longitude,50)
            compare(map.gesture.panActive, false)
        }

        function test_disable_onPanStarted_with_disabled()
        {
            map.disableOnPanStartedWithDisabled = true
            disable_onPanStarted()
        }

        function test_disable_onPanStarted_with_nogesture()
        {
            map.disableOnPanStartedWithNoGesture = true
            disable_onPanStarted()
        }
    }
}
