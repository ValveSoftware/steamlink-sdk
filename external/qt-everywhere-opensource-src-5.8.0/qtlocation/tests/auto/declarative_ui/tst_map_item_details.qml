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
import QtPositioning 5.5
import QtLocation 5.6
import QtLocation.Test 5.6

Item {
    id: page
    x: 0; y: 0;
    width: 240
    height: 240
    Plugin { id: testPlugin
             name : "qmlgeo.test.plugin"
             allowExperimental: true
             parameters: [ PluginParameter { name: "finishRequestImmediately"; value: true}]
    }

    property variant mapDefaultCenter: QtPositioning.coordinate(20, 20)

    property variant datelineCoordinate: QtPositioning.coordinate(20, 180)
    property variant datelineCoordinateLeft: QtPositioning.coordinate(20, 170)
    property variant datelineCoordinateRight: QtPositioning.coordinate(20, -170)

    MapPolygon {
        id: extMapPolygon
        color: 'darkgrey'
        path: [
            { latitude: 25, longitude: 5 },
            { latitude: 20, longitude: 10 }
        ]
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            SignalSpy { id: extMapPolygonClicked; target: parent; signalName: "clicked" }
        }
        SignalSpy {id: extMapPolygonPathChanged; target: parent; signalName: "pathChanged"}
        SignalSpy {id: extMapPolygonColorChanged; target: parent; signalName: "colorChanged"}
        SignalSpy {id: extMapPolygonBorderWidthChanged; target: parent.border; signalName: "widthChanged"}
        SignalSpy {id: extMapPolygonBorderColorChanged; target: parent.border; signalName: "colorChanged"}
    }

    property variant polyCoordinate: QtPositioning.coordinate(15, 6)

    MapPolygon {
        id: extMapPolygon0
        color: 'darkgrey'
    }

    MapPolyline {
        id: extMapPolyline0
    }

    MapPolyline {
        id: extMapPolyline
        path: [
            { latitude: 25, longitude: 5 },
            { latitude: 20, longitude: 10 }
        ]
        SignalSpy {id: extMapPolylineColorChanged; target: parent.line; signalName: "colorChanged"}
        SignalSpy {id: extMapPolylineWidthChanged; target: parent.line; signalName: "widthChanged"}
        SignalSpy {id: extMapPolylinePathChanged; target: parent; signalName: "pathChanged"}
    }

    MapRectangle {
        id: extMapRectDateline
        color: 'darkcyan'
        topLeft {
            latitude: 20
            longitude: 175
        }
        bottomRight {
            latitude: 10
            longitude: -175
        }
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            preventStealing: true
        }
    }

    MapCircle {
        id: extMapCircleDateline
        color: 'darkmagenta'
        center {
            latitude: 20
            longitude: 180
        }
        radius: 400000
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            preventStealing: true
        }
    }

    MapQuickItem {
        id: extMapQuickItemDateline
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            preventStealing: true
        }
        coordinate {
            latitude: 20
            longitude: 175
        }
        sourceItem: Rectangle {
            color: 'darkgreen'
            width: 20
            height: 20
        }
    }

    MapPolygon {
        id: extMapPolygonDateline
        color: 'darkmagenta'
        path: [
            { latitude: 20, longitude: 175 },
            { latitude: 20, longitude: -175 },
            { latitude: 10, longitude: -175 },
            { latitude: 10, longitude: 175 }
        ]
        MouseArea {
            anchors.fill: parent
            drag.target: parent
            preventStealing: true
        }
    }

    MapPolyline {
        id: extMapPolylineDateline
        line.width : 3
        path: [
            { latitude: 20, longitude: 175 },
            { latitude: 25, longitude: -175 }
        ]
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
    }

    MapRoute {
        id: extMapRouteDateline
        line.color: 'yellow'
        route: Route {
            path: [
                { latitude: 25, longitude: 175 },
                { latitude: 20, longitude: -175 }
            ]
        }
    }

    MapRectangle {
        id: extMapRectEdge
        color: 'darkcyan'
        topLeft {
            latitude: 20
            longitude: -15
        }
        bottomRight {
            latitude: 10
            longitude: -5
        }
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
    }

    MapCircle {
        id: extMapCircleEdge
        color: 'darkmagenta'
        center {
            latitude: 20
            longitude: -15
        }
        radius: 400000
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
    }

    MapQuickItem {
        id: extMapQuickItemEdge
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
        coordinate {
            latitude: 20
            longitude: -15
        }
        sourceItem: Rectangle {
            color: 'darkgreen'
            width: 20
            height: 20
        }
    }

    MapPolygon {
        id: extMapPolygonEdge
        color: 'darkmagenta'
        path: [
            { latitude: 20, longitude: -15 },
            { latitude: 20, longitude: -5 },
            { latitude: 10, longitude: -5 },
            { latitude: 10, longitude: -15 }
        ]
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
    }

    MapPolyline {
        id: extMapPolylineEdge
        line.width : 3
        path: [
            { latitude: 20, longitude: -15 },
            { latitude: 25, longitude: -5 }
        ]
        MouseArea {
            anchors.fill: parent
            drag.target: parent
        }
    }

    MapRoute {
        id: extMapRouteEdge
        line.color: 'yellow'
        route: Route {
            path: [
                { latitude: 25, longitude: -15 },
                { latitude: 20, longitude: -5 }
            ]
        }
    }

    Map {
        id: map;
        x: 20; y: 20; width: 200; height: 200
        center: mapDefaultCenter
        plugin: testPlugin;
    }

    Text {id: progressText}

    TestCase {
        name: "MapItemDetails"
        when: windowShown

    /*

     (0,0)   ---------------------------------------------------- (240,0)
             | no map                                           |
             |    (20,20)                                       |
     (0,20)  |    ------------------------------------------    | (240,20)
             |    |                                        |    |
             |    |  map                                   |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                   (lat 20, lon 20)     |    |
             |    |                     x                  |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    ------------------------------------------    |
             |                                                  |
     (0,240) ---------------------------------------------------- (240,240)

     */
        function initTestCase()
        {
            // sanity check that the coordinate conversion works
            var mapcenter = map.fromCoordinate(map.center)
            verify (fuzzy_compare(mapcenter.x, 100, 2))
            verify (fuzzy_compare(mapcenter.y, 100, 2))
        }

        function init()
        {
            map.clearMapItems()
            map.zoomLevel = 3
            extMapPolygon.border.width = 1.0
            extMapPolygonClicked.clear()
            extMapPolylineColorChanged.clear()
            extMapPolylineWidthChanged.clear()
            extMapPolylinePathChanged.clear()
            extMapPolygonPathChanged.clear()
            extMapPolygonColorChanged.clear()
            extMapPolygonBorderColorChanged.clear()
            extMapPolygonBorderWidthChanged.clear()
        }

        function test_polygon()
        {
            map.center = extMapPolygon.path[1]
            var point = map.fromCoordinate(extMapPolygon.path[1])
            map.addMapItem(extMapPolygon)
            verify(LocationTestHelper.waitForPolished(map))
            verify(extMapPolygon.path.length == 2)
            mouseClick(map, point.x - 5, point.y)
            compare(extMapPolygonClicked.count, 0)
            map.addMapItem(extMapPolygon0) // mustn't crash or ill-behave
            verify(extMapPolygon0.path.length == 0)
            extMapPolygon.addCoordinate(polyCoordinate)
            verify(extMapPolygon.path.length == 3)
            verify(LocationTestHelper.waitForPolished(map))
            mouseClick(map, point.x - 5, point.y)
            tryCompare(extMapPolygonClicked, "count", 1)

            extMapPolygon.path[0].latitude = 10
            verify(extMapPolygon.path[0].latitude, 10)
            extMapPolygon.path[0].latitude = polyCoordinate.latitude
            verify(extMapPolygon.path[0].latitude, 15)
            extMapPolygon.path[0].longitude = 2
            verify(extMapPolygon.path[0].longitude, 2)
            extMapPolygon.path[0].longitude = polyCoordinate.longitude
            verify(extMapPolygon.path[0].longitude, 6)

            extMapPolygon.removeCoordinate(polyCoordinate)
            verify(extMapPolygon.path.length == 2)
            extMapPolygon.removeCoordinate(extMapPolygon.path[1])
            verify(extMapPolygon.path.length == 1)
            extMapPolygon.removeCoordinate(extMapPolygon.path[0])
            verify(extMapPolygon.path.length == 0)
        }

        function test_polyline()
        {
            compare (extMapPolyline.line.width, 1.0)
            var point = map.fromCoordinate(extMapPolyline.path[1])
            map.addMapItem(extMapPolyline0) // mustn't crash or ill-behave
            verify(extMapPolyline0.path.length == 0)
            map.addMapItem(extMapPolyline)
            verify(extMapPolyline.path.length == 2)
            extMapPolyline.addCoordinate(polyCoordinate)
            verify(extMapPolyline.path.length == 3)
            extMapPolyline.addCoordinate(extMapPolyline.path[0])
            verify(extMapPolyline.path.length == 4)

            extMapPolyline.path[0].latitude = 10
            verify(extMapPolyline.path[0].latitude, 10)
            extMapPolyline.path[0].latitude = polyCoordinate.latitude
            verify(extMapPolyline.path[0].latitude, 15)
            extMapPolyline.path[0].longitude = 2
            verify(extMapPolyline.path[0].longitude, 2)
            extMapPolyline.path[0].longitude = polyCoordinate.longitude
            verify(extMapPolyline.path[0].longitude, 6)

            // TODO when line rendering is ready
            //mouseClick(map, point.x - 5, point.y)
            //compare(extMapPolylineClicked.count, 1)
            extMapPolyline.removeCoordinate(extMapPolyline.path[0])
            verify(extMapPolyline.path.length == 3)
            extMapPolyline.removeCoordinate(polyCoordinate)
            verify(extMapPolyline.path.length == 2)
            extMapPolyline.removeCoordinate(extMapPolyline.path[1])
            verify(extMapPolyline.path.length == 1)
            extMapPolyline.removeCoordinate(extMapPolyline.path[0])
            verify(extMapPolyline.path.length == 0)
        }

    /*

     (0,0)   ---------------------------------------------------- (600,0)
             | no map                                           |
             |    (20,20)                                       |
     (0,20)  |    ------------------------------------------    | (600,20)
             |    |                                        |    |
             |    |  map                                   |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                   (lat 20, lon 180)    |    |
             |    |                     x                  |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    |                                        |    |
             |    ------------------------------------------    |
             |                                                  |
     (0,240) ---------------------------------------------------- (600,240)

     */
        function test_dateline() {
            map.center = datelineCoordinate
            map.zoomLevel = 2.2
            var inspectionTime = 0 // change this to inspect the behavior.

            // rectangle
            // item spanning across dateline
            map.addMapItem(extMapRectDateline)
            verify(extMapRectDateline.topLeft.longitude == 175)
            verify(extMapRectDateline.bottomRight.longitude == -175)
            var point = map.fromCoordinate(extMapRectDateline.topLeft)
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            // move item away from dataline by directly setting its coords
            extMapRectDateline.bottomRight.longitude = datelineCoordinateRight.longitude
            point = map.fromCoordinate(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            // move item edge onto dateline
            extMapRectDateline.topLeft.longitude = datelineCoordinate.longitude
            point = map.fromCoordinate(extMapRectDateline.topLeft)
            verify(point.x == map.width / 2.0)
            // drag item back onto dateline
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            mousePress(map, point.x + 5, point.y + 5)
            var i
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y + 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y + 5)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            point = map.fromCoordinate(extMapRectDateline.topLeft)
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            map.removeMapItem(extMapRectDateline)

            // circle
            map.addMapItem(extMapCircleDateline)
            verify(extMapCircleDateline.center.longitude === 180)
            map.center = datelineCoordinate
            point = map.fromCoordinate(extMapCircleDateline.center)
            verify(point.x == map.width / 2.0) // center of the screen
            visualInspectionPoint()
            extMapCircleDateline.center.longitude = datelineCoordinateRight.longitude // -170, moving the circle to the right
            point = map.fromCoordinate(extMapCircleDateline.center)
            verify(LocationTestHelper.waitForPolished(map))
            verify(point.x > map.width / 2.0)
            visualInspectionPoint(inspectionTime)
            mousePress(map, point.x, point.y)
            for (i=0; i < 50; i += 4) {
                wait(1)
                mouseMove(map, point.x - i, point.y)
            }
            mouseRelease(map, point.x - i, point.y)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            point = map.fromCoordinate(extMapCircleDateline.center)
            visualInspectionPoint()
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapCircleDateline)

            // quickitem
            map.addMapItem(extMapQuickItemDateline)
            map.center = datelineCoordinate
            verify(extMapQuickItemDateline.coordinate.longitude === 175)
            point = map.fromCoordinate(extMapQuickItemDateline.coordinate)
            verify(point.x < map.width / 2.0)
            visualInspectionPoint()
            extMapQuickItemDateline.coordinate.longitude = datelineCoordinateRight.longitude
            point = map.fromCoordinate(extMapQuickItemDateline.coordinate)
            verify(point.x > map.width / 2.0)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            mousePress(map, point.x + 5, point.y + 5)
            for (i=0; i < 64; i += 5) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y + 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y + 5)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            point = map.fromCoordinate(extMapQuickItemDateline.coordinate)
            visualInspectionPoint()
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapQuickItemDateline)

            // polygon
            map.addMapItem(extMapPolygonDateline)
            map.center = datelineCoordinate
            verify(extMapPolygonDateline.path[0].longitude == 175)
            verify(extMapPolygonDateline.path[1].longitude == -175)
            verify(extMapPolygonDateline.path[2].longitude == -175)
            verify(extMapPolygonDateline.path[3].longitude == 175)
            point = map.fromCoordinate(extMapPolygonDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[3])
            verify(point.x < map.width / 2.0)
            extMapPolygonDateline.path[1].longitude = datelineCoordinateRight.longitude
            point = map.fromCoordinate(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            extMapPolygonDateline.path[2].longitude = datelineCoordinateRight.longitude
            point = map.fromCoordinate(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            var path = extMapPolygonDateline.path;
            path[0].longitude = datelineCoordinate.longitude;
            extMapPolygonDateline.path = path;
            point = map.fromCoordinate(extMapPolygonDateline.path[0])
            verify(point.x == map.width / 2.0)
            path = extMapPolygonDateline.path;
            path[3].longitude = datelineCoordinate.longitude;
            extMapPolygonDateline.path = path;
            point = map.fromCoordinate(extMapPolygonDateline.path[3])
            verify(point.x == map.width / 2.0)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            mousePress(map, point.x + 5, point.y - 5)
            for (i=0; i < 16; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y - 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y - 5)
            verify(LocationTestHelper.waitForPolished(map))
            visualInspectionPoint(inspectionTime)
            point = map.fromCoordinate(extMapPolygonDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            point = map.fromCoordinate(extMapPolygonDateline.path[3])
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapPolygonDateline)

            // polyline
            map.addMapItem(extMapPolylineDateline)
            map.center = datelineCoordinate
            verify(extMapPolylineDateline.path[0].longitude == 175)
            verify(extMapPolylineDateline.path[1].longitude == -175)
            point = map.fromCoordinate(extMapPolylineDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapPolylineDateline.path[1])
            verify(point.x > map.width / 2.0)
            extMapPolylineDateline.path[1].longitude = datelineCoordinateRight.longitude
            point = map.fromCoordinate(extMapPolylineDateline.path[1])
            verify(point.x > map.width / 2.0)
            var path = extMapPolygonDateline.path;
            path[0].longitude = datelineCoordinate.longitude;
            extMapPolylineDateline.path = path;
            point = map.fromCoordinate(extMapPolylineDateline.path[0])
            verify(point.x == map.width / 2.0)
            map.removeMapItem(extMapPolylineDateline)

            // map route
            // (does not support setting of path coords)
            map.addMapItem(extMapRouteDateline)
            verify(extMapRouteDateline.route.path[0].longitude == 175)
            verify(extMapRouteDateline.route.path[1].longitude == -175)
            point = map.fromCoordinate(extMapRouteDateline.route.path[0])
            verify(point.x < map.width / 2.0)
            point = map.fromCoordinate(extMapRouteDateline.route.path[1])
            verify(point.x > map.width / 2.0)
            map.removeMapItem(extMapRouteDateline)
        }

        function fuzzy_compare(val, ref, tol) {
            var tolerance = 2
            if (tol !== undefined)
                tolerance = tol
            if ((val >= ref - tolerance) && (val <= ref + tolerance))
                return true;
            console.log('map fuzzy cmp returns false for value, ref, tolerance: ' + val + ', ' + ref + ', ' + tolerance)
            return false;
        }
        // call to visualInspectionPoint testcase (for dev time visual inspection)
        function visualInspectionPoint(time) {
            var waitTime = 0 // 300
            if (time !== undefined)
                waitTime = time
            if (waitTime > 0) {
                console.log('halting for ' + waitTime + ' milliseconds')
                wait (waitTime)
            }
        }
    }
}
