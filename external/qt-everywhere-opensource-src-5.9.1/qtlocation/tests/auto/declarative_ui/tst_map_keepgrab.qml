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
    width: 200
    height: 200
    Plugin { id: testPlugin; name: "qmlgeo.test.plugin"; allowExperimental: true }


    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: flickable.width * 4; contentHeight: flickable.height

        Map {
            id: map
            x: flickable.width
            height: flickable.height
            width:flickable.width
            plugin: testPlugin
        }
    }

    SignalSpy { id: mapPanStartedSpy; target: map.gesture; signalName: 'panStarted' }
    SignalSpy { id: mapPanFinishedSpy; target: map.gesture; signalName: 'panFinished' }
    SignalSpy { id: flickStartedSpy; target: flickable; signalName: 'flickStarted' }
    SignalSpy { id: flickEndedSpy; target: flickable; signalName: 'flickEnded' }
    SignalSpy { id: preventStealingChangedSpy; target: map.gesture; signalName: 'preventStealingChanged' }


    TestCase {
        when: windowShown && map.mapReady
        name: "MapKeepGrabAndPreventSteal"

        function initTestCase()
        {
            compare(map.gesture.preventStealing, false)
        }

        function init()
        {
            map.gesture.acceptedGestures = MapGestureArea.PanGesture | MapGestureArea.FlickGesture;
            map.gesture.flickDeceleration = 500
            map.zoomLevel = 1
            map.center = QtPositioning.coordinate(50,50)
            map.gesture.preventStealing = false
            flickable.contentX = 0
            flickable.contentY = 0
            mapPanStartedSpy.clear()
            mapPanFinishedSpy.clear()
            flickStartedSpy.clear()
            flickEndedSpy.clear()
            preventStealingChangedSpy.clear()
        }

        function flick()
        {
            var i = 0
            mousePress(flickable, flickable.width - 1, 0)
            for (i = flickable.width; i > 0; i -= 5) {
                wait(5)
                mouseMove(flickable, i, 0, 0, Qt.LeftButton);
            }
            mouseRelease(flickable, i, 0)
        }

        function pan()
        {
            var i = 0
            mousePress(map, 0, 0)
            for (i = 0; i < flickable.width; i += 5) {
                wait(5)
                mouseMove(map, i, 0, 0, Qt.LeftButton);
            }
            mouseRelease(map, i, 0)
        }

        function test_flick()
        {
            var center = QtPositioning.coordinate(map.center.latitude,map.center.longitude)
            flick() //flick flickable
            tryCompare(flickStartedSpy,"count",1)
            pan() //pan map
            tryCompare(flickStartedSpy,"count",2) // both directions
            tryCompare(flickEndedSpy,"count",1)
            tryCompare(mapPanStartedSpy,"count", 0)
            tryCompare(mapPanFinishedSpy,"count", 0)
            //map should not change
            verify(center == map.center)
        }

        function test_map_grab()
        {
             var center = QtPositioning.coordinate(map.center.latitude,map.center.longitude)
            pan() //pan map
            tryCompare(mapPanStartedSpy,"count",1)
            tryCompare(mapPanFinishedSpy, "count", 1)

            compare(flickStartedSpy.count, 0)
            compare(flickEndedSpy.count, 0)
            //map should change
            verify(center != map.center)
        }

        function test_map_preventsteal()
        {
            map.gesture.preventStealing = false
            compare(preventStealingChangedSpy.count, 0)
            map.gesture.preventStealing = true
            compare(preventStealingChangedSpy.count, 1)

            var center = QtPositioning.coordinate(map.center.latitude,map.center.longitude)
            flick() //flick flickable
            tryCompare(flickStartedSpy,"count",1)
            pan() //pan map
            tryCompare(flickStartedSpy,"count",1) // both directions
            tryCompare(flickEndedSpy,"count",1)
            tryCompare(mapPanStartedSpy,"count", 1)
            tryCompare(mapPanFinishedSpy,"count", 1)
            //map should not change
            verify(center != map.center)
        }
    }
}
