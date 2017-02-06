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
import QtLocation 5.6
import QtPositioning 5.5
import QtLocation.Test 5.6

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

Item {
    id: page
    x: 0; y: 0;
    width: 240
    height: 240
    Plugin { id: testPlugin; name : "qmlgeo.test.plugin"; allowExperimental: true }

    property variant someCoordinate1: QtPositioning.coordinate(15, 15)
    property variant someCoordinate2: QtPositioning.coordinate(16, 16)

    Route { id: someRoute;
        path: [
            { latitude: 22, longitude: 15 },
            { latitude: 21, longitude: 16 },
            { latitude: 23, longitude: 17 }
        ]
    }
    Item { id: someItem }

    MapCircle {
        id: extMapCircle
        center {
            latitude: 35
            longitude: 15
        }
        color: 'firebrick'
        radius: 600000
        MouseArea {
            anchors.fill: parent
            SignalSpy { id: extMapCircleClicked; target: parent; signalName: "clicked" }
        }
    }

    MapQuickItem {
        id: extMapQuickItem
        MouseArea {
            anchors.fill: parent
            SignalSpy { id: extMapQuickItemClicked; target: parent; signalName: "clicked" }
        }
        coordinate {
            latitude: 35
            longitude: 33
        }
        sourceItem: Rectangle {
            color: 'darkblue'
            width: 40
            height: 20
        }
    }

    Map {
        id: map;
        x: 20; y: 20; width: 200; height: 200
        zoomLevel: 9
        plugin: testPlugin;

        MapRectangle {
            id: preMapRect
            MouseArea {
                id: preMapRectMa
                anchors.fill: parent
                drag.target: parent
                preventStealing: true
                SignalSpy { id: preMapRectClicked; target: parent; signalName: "clicked" }
                SignalSpy { id: preMapRectActiveChanged; target: parent.drag; signalName: "activeChanged" }
            }
            SignalSpy {id: preMapRectTopLeftChanged; target: parent; signalName: "topLeftChanged" }
            SignalSpy {id: preMapRectBottomRightChanged; target: parent; signalName: "bottomRightChanged" }
            SignalSpy {id: preMapRectColorChanged; target: parent; signalName: "colorChanged"}
        }
        MapCircle {
            id: preMapCircle
            MouseArea {
                id: preMapCircleMa
                anchors.fill: parent
                drag.target: parent
                preventStealing: true
                SignalSpy { id: preMapCircleClicked; target: parent; signalName: "clicked" }
                SignalSpy { id: preMapCircleActiveChanged; target: parent.drag; signalName: "activeChanged" }
            }
            SignalSpy {id: preMapCircleCenterChanged; target: parent; signalName: "centerChanged"}
            SignalSpy {id: preMapCircleColorChanged; target: parent; signalName: "colorChanged"}
            SignalSpy {id: preMapCircleRadiusChanged; target: parent; signalName: "radiusChanged"}
            SignalSpy {id: preMapCircleBorderColorChanged; target: parent.border; signalName: "colorChanged"}
            SignalSpy {id: preMapCircleBorderWidthChanged; target: parent.border; signalName: "widthChanged"}
        }
        MapQuickItem {
            id: preMapQuickItem
            MouseArea {
                id: preMapQuickItemMa
                anchors.fill: parent
                drag.target: parent
                preventStealing: true
                SignalSpy { id: preMapQuickItemClicked; target: parent; signalName: "clicked" }
                SignalSpy { id: preMapQuickItemActiveChanged; target: parent.drag; signalName: "activeChanged" }
            }
            sourceItem: Rectangle {
                id: preMapQuickItemSource
                color: 'darkgreen'
                width: 20
                height: 20
            }
            SignalSpy { id: preMapQuickItemCoordinateChanged; target: parent; signalName: "coordinateChanged"}
            SignalSpy { id: preMapQuickItemAnchorPointChanged; target: parent; signalName: "anchorPointChanged"}
            SignalSpy { id: preMapQuickItemZoomLevelChanged; target: parent; signalName: "zoomLevelChanged"}
            SignalSpy { id: preMapQuickItemSourceItemChanged; target: parent; signalName: "sourceItemChanged"}
        }
        MapPolygon {
            id: preMapPolygon
            color: 'darkgrey'
            border.width: 0
            path: [
                { latitude: 25, longitude: 5 },
                { latitude: 20, longitude: 10 },
                { latitude: 15, longitude: 6 }
            ]
            MouseArea {
                anchors.fill: parent
                drag.target: parent
                SignalSpy { id: preMapPolygonClicked; target: parent; signalName: "clicked" }
            }
            SignalSpy {id: preMapPolygonPathChanged; target: parent; signalName: "pathChanged"}
            SignalSpy {id: preMapPolygonColorChanged; target: parent; signalName: "colorChanged"}
            SignalSpy {id: preMapPolygonBorderWidthChanged; target: parent.border; signalName: "widthChanged"}
            SignalSpy {id: preMapPolygonBorderColorChanged; target: parent.border; signalName: "colorChanged"}
        }
        MapPolyline {
            id: preMapPolyline
            line.color: 'darkred'
            path: [
                { latitude: 25, longitude: 15 },
                { latitude: 20, longitude: 19 },
                { latitude: 15, longitude: 16 }
            ]
            SignalSpy {id: preMapPolylineColorChanged; target: parent.line; signalName: "colorChanged"}
            SignalSpy {id: preMapPolylineWidthChanged; target: parent.line; signalName: "widthChanged"}
            SignalSpy {id: preMapPolylinePathChanged; target: parent; signalName: "pathChanged"}
        }
        MapRoute {
            id: preMapRoute
            line.color: 'yellow'
            // don't try this at home - route is not user instantiable
            route: Route {
                path: [
                    { latitude: 25, longitude: 14 },
                    { latitude: 20, longitude: 18 },
                    { latitude: 15, longitude: 15 }
                ]
            }
            SignalSpy {id: preMapRouteRouteChanged; target: parent; signalName: "routeChanged"}
            SignalSpy {id: preMapRouteLineWidthChanged; target: parent.line; signalName: "widthChanged"}
            SignalSpy {id: preMapRouteLineColorChanged; target: parent.line; signalName: "colorChanged"}
        }
    }
    TestCase {
        name: "MapItems"
        when: windowShown

        function initTestCase()
        {
            // sanity check that the coordinate conversion works, as
            // rest of the case relies on it. for robustness cut
            // a little slack with fuzzy compare
            var mapcenter = map.fromCoordinate(map.center)
            verify (fuzzy_compare(mapcenter.x, 100, 2))
            verify (fuzzy_compare(mapcenter.y, 100, 2))
        }

        function init()
        {
            map.center = QtPositioning.coordinate(20, 20)
            preMapCircle.center = QtPositioning.coordinate(10,30)
            preMapCircle.border.width = 0
            preMapCircle.color = 'red'
            preMapCircle.radius = 10000
            preMapCircleClicked.clear()
            preMapCircleCenterChanged.clear()
            preMapCircleColorChanged.clear()
            preMapCircleRadiusChanged.clear()
            preMapCircleBorderColorChanged.clear()
            preMapCircleBorderWidthChanged.clear()

            preMapRect.color = 'red'
            preMapRect.border.width = 0
            preMapRect.topLeft = QtPositioning.coordinate(20, 20)
            preMapRect.bottomRight = QtPositioning.coordinate(10, 30)
            preMapRectTopLeftChanged.clear()
            preMapRectBottomRightChanged.clear()
            preMapRectColorChanged.clear()
            preMapRectClicked.clear()
            preMapRectActiveChanged.clear()

            preMapQuickItem.sourceItem = preMapQuickItemSource
            preMapQuickItem.zoomLevel = 0
            preMapQuickItem.coordinate =  QtPositioning.coordinate(35, 3)
            preMapQuickItemClicked.clear()
            preMapQuickItem.anchorPoint = Qt.point(0,0)
            preMapQuickItemCoordinateChanged.clear()
            preMapQuickItemAnchorPointChanged.clear()
            preMapQuickItemZoomLevelChanged.clear()
            preMapQuickItemSourceItemChanged.clear()

            preMapPolygonClicked.clear()
            preMapPolylineColorChanged.clear()
            preMapPolylineWidthChanged.clear()
            preMapPolylinePathChanged.clear()
            preMapPolygonPathChanged.clear()
            preMapPolygonColorChanged.clear()
            preMapPolygonBorderColorChanged.clear()
            preMapPolygonBorderWidthChanged.clear()
            preMapRouteRouteChanged.clear()
            preMapRouteLineColorChanged.clear()
            preMapRouteLineWidthChanged.clear()
            verify(LocationTestHelper.waitForPolished(map))
        }

        function test_items_on_map()
        {
            // click rect
            map.center = preMapRect.topLeft
            verify(LocationTestHelper.waitForPolished(map))
            var point = map.fromCoordinate(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            tryCompare(preMapRectClicked, "count", 1)
            mouseClick(map, 1, 1) // no item hit
            tryCompare(preMapRectClicked, "count", 1)
            compare(preMapCircleClicked.count, 0)

            // click circle, overlaps and is above rect
            map.center = preMapCircle.center
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            tryCompare(preMapCircleClicked, "count", 1)
            compare(preMapRectClicked.count, 1)

            // click within circle bounding rect but not inside the circle geometry
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            tryCompare(preMapRectClicked, "count", 2)
            compare(preMapCircleClicked.count, 1)

            // click quick item
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            tryCompare(preMapQuickItemClicked, "count", 1)

            // click polygon
            compare (preMapPolygonClicked.count, 0)
            map.center = preMapPolygon.path[1]
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapPolygon.path[1])
            mouseClick(map, point.x - 5, point.y)
            tryCompare(preMapPolygonClicked, "count", 1)
        }

        function test_no_items_on_map()
        {
            // remove items and repeat clicks to verify they are gone
            map.clearMapItems()
            compare (map.mapItems.length, 0)
            map.center = preMapRect.topLeft
            var point = map.fromCoordinate(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapRectClicked.count, 0)
            verify(LocationTestHelper.waitForPolished(map))
            map.center = preMapCircle.center
            point = map.fromCoordinate(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            compare(preMapRectClicked.count, 0)
            compare(preMapCircleClicked.count, 0)
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            compare(preMapRectClicked.count, 0)
            compare(preMapCircleClicked.count, 0)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            point = map.fromCoordinate(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapPolygon.path[1]
            point = map.fromCoordinate(preMapPolygon.path[1])
            mouseClick(map, point.x - 5, point.y)
            compare(preMapPolygonClicked.count, 0)

            // re-add items and verify they are back
            // note: addition order is significant
            map.addMapItem(preMapRect)
            map.addMapItem(preMapCircle)
            map.addMapItem(preMapQuickItem)
            map.addMapItem(preMapPolygon)
            map.addMapItem(preMapPolyline)
            map.addMapItem(preMapRoute)
            compare (map.mapItems.length, 6)

            map.center = preMapRect.topLeft
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            tryCompare(preMapRectClicked, "count", 1)
            map.center = preMapCircle.center
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            tryCompare(preMapRectClicked, "count", 1)
            compare(preMapCircleClicked.count, 1)
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            verify(LocationTestHelper.waitForPolished(map))
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            tryCompare(preMapRectClicked, "count", 2)
            compare(preMapCircleClicked.count, 1)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            tryCompare(preMapQuickItemClicked, "count", 1)
            map.center = preMapPolygon.path[1]
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapPolygon.path[1])
            mouseClick(map, point.x - 5, point.y)
            tryCompare(preMapPolygonClicked, "count", 1)


            // item clips to map. not sure if this is sensible test
            map.addMapItem(extMapCircle)
            map.center = extMapCircle.center
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(extMapCircle.center)
            mouseClick(map, point.x, point.y)
            tryCompare(extMapCircleClicked, "count", 1)
            mouseClick(map, point.x, -5)
            tryCompare(extMapCircleClicked, "count", 1)
            map.removeMapItem(extMapCircle)

            map.addMapItem(extMapQuickItem)
            map.center = extMapQuickItem.coordinate
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(extMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            tryCompare(extMapQuickItemClicked, "count", 1)
            mouseClick(map, map.width + 5, point.y + 5)
            tryCompare(extMapQuickItemClicked, "count", 1)
            map.removeMapItem(extMapQuickItem)
        }

        function test_drag()
        {
            // basic drags, drag rectangle
            compare (preMapRectActiveChanged.count, 0)
            map.center = preMapRect.topLeft
            verify(LocationTestHelper.waitForPolished(map))
            var i
            var point = map.fromCoordinate(preMapRect.topLeft)
            var targetCoordinate = map.toCoordinate(51, 51)
            mousePress(map, point.x + 5, point.y + 5)
            for (i = 0; i < 50; i += 1) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y + 5 - i)
            }
            mouseRelease(map, point.x + 5 - i, point.y + 5 - i)
            compare (preMapRectActiveChanged.count, 2)
            verify(preMapRectTopLeftChanged.count > 1)
            verify(preMapRectBottomRightChanged.count === preMapRectTopLeftChanged.count)
            verify(fuzzy_compare(preMapRect.topLeft.latitude, targetCoordinate.latitude, 0.2))
            verify(fuzzy_compare(preMapRect.topLeft.longitude, targetCoordinate.longitude, 0.2))
            var latH = preMapRect.bottomRight.latitude - preMapRect.topLeft.latitude
            var lonW = preMapRect.bottomRight.longitude - preMapRect.topLeft.longitude
            verify(fuzzy_compare(preMapRect.bottomRight.latitude, preMapRect.topLeft.latitude + latH, 0.1))
            verify(fuzzy_compare(preMapRect.bottomRight.longitude, preMapRect.topLeft.longitude + lonW, 0.1))

            // drag circle
            compare (preMapCircleActiveChanged.count, 0)
            map.center = preMapCircle.center
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapCircle.center)
            targetCoordinate = map.toCoordinate(51, 51)
            mousePress(map, point.x, point.y)
            for (i = 0; i < 50; i += 1) {
                wait(1)
                mouseMove(map, point.x - i, point.y - i)
            }
            mouseRelease(map, point.x - i, point.y - i)
            verify(LocationTestHelper.waitForPolished(map))
            compare(preMapRectActiveChanged.count, 2)
            compare(preMapCircleActiveChanged.count, 2)
            verify(preMapCircleCenterChanged.count > 1)
            verify(fuzzy_compare(preMapCircle.center.latitude, targetCoordinate.latitude, 0.2))
            verify(fuzzy_compare(preMapCircle.center.longitude, targetCoordinate.longitude, 0.2))

            // drag quick item
            compare (preMapQuickItemActiveChanged.count, 0)
            map.center = preMapQuickItem.coordinate
            verify(LocationTestHelper.waitForPolished(map))
            point = map.fromCoordinate(preMapQuickItem.coordinate)
            targetCoordinate = map.toCoordinate(51, 51)
            mousePress(map, point.x + 5, point.y + 5)
            for (i = 0; i < 50; i += 1) {
                wait(1)
                mouseMove(map, point.x - i, point.y - i)
            }
            mouseRelease(map, point.x - i, point.y - i)
            verify(LocationTestHelper.waitForPolished(map))
            compare(preMapQuickItemActiveChanged.count, 2)
            verify(preMapQuickItemCoordinateChanged.count > 1)
            verify(fuzzy_compare(preMapQuickItem.coordinate.latitude, targetCoordinate.latitude, 0.2))
            verify(fuzzy_compare(preMapQuickItem.coordinate.longitude, targetCoordinate.longitude, 0.2))
        }

        function test_basic_items_properties()
        {
            // circle
            preMapCircle.center = someCoordinate1
            compare (preMapCircleCenterChanged.count, 1)
            preMapCircle.center = someCoordinate1
            compare (preMapCircleCenterChanged.count, 1)
            preMapCircle.color = 'blue'
            compare (preMapCircleColorChanged.count, 1)
            preMapCircle.color = 'blue'
            compare (preMapCircleColorChanged.count, 1)
            preMapCircle.radius  = 50
            compare (preMapCircleRadiusChanged.count, 1)
            preMapCircle.radius  = 50
            compare (preMapCircleRadiusChanged.count, 1)
            preMapCircle.border.color = 'blue'
            compare(preMapCircleBorderColorChanged.count, 1)
            preMapCircle.border.color = 'blue'
            compare(preMapCircleBorderColorChanged.count, 1)
            preMapCircle.border.width = 5
            compare(preMapCircleBorderWidthChanged.count, 1)
            preMapCircle.border.width = 5
            compare(preMapCircleBorderWidthChanged.count, 1)

            // rectangle
            preMapRect.topLeft = someCoordinate1
            compare (preMapRectTopLeftChanged.count, 1)
            compare (preMapRectBottomRightChanged.count, 0)
            preMapRect.bottomRight = someCoordinate2
            compare (preMapRectTopLeftChanged.count, 1)
            compare (preMapRectBottomRightChanged.count, 1)
            preMapRect.bottomRight = someCoordinate2
            preMapRect.topLeft = someCoordinate1
            compare (preMapRectTopLeftChanged.count, 1)
            compare (preMapRectBottomRightChanged.count, 1)
            preMapRect.color = 'blue'
            compare (preMapRectColorChanged.count, 1)
            preMapRect.color = 'blue'
            compare (preMapRectColorChanged.count, 1)

            // polyline
            preMapPolyline.line.width = 5
            compare (preMapPolylineWidthChanged.count, 1)
            preMapPolyline.line.width = 5
            compare (preMapPolylineWidthChanged.count, 1)
            preMapPolyline.line.color = 'blue'
            compare(preMapPolylineColorChanged.count, 1)
            preMapPolyline.line.color = 'blue'
            compare(preMapPolylineColorChanged.count, 1)
            preMapPolyline.addCoordinate(someCoordinate1)
            compare (preMapPolylinePathChanged.count, 1)
            preMapPolyline.addCoordinate(someCoordinate1)
            compare (preMapPolylinePathChanged.count, 2)
            preMapPolyline.removeCoordinate(someCoordinate1)
            compare (preMapPolylinePathChanged.count, 3)
            preMapPolyline.removeCoordinate(someCoordinate1)
            compare (preMapPolylinePathChanged.count, 4)
            preMapPolyline.removeCoordinate(someCoordinate1)
            compare (preMapPolylinePathChanged.count, 4)

            // polygon
            preMapPolygon.border.width = 5
            compare (preMapPolylineWidthChanged.count, 1)
            preMapPolygon.border.width = 5
            compare (preMapPolylineWidthChanged.count, 1)
            preMapPolygon.border.color = 'blue'
            compare(preMapPolylineColorChanged.count, 1)
            preMapPolygon.border.color = 'blue'
            preMapPolygon.color = 'blue'
            compare (preMapPolygonColorChanged.count, 1)
            preMapPolygon.color = 'blue'
            compare (preMapPolygonColorChanged.count, 1)
            preMapPolygon.addCoordinate(someCoordinate1)
            compare (preMapPolygonPathChanged.count, 1)
            preMapPolygon.addCoordinate(someCoordinate1)
            compare (preMapPolygonPathChanged.count, 2)
            preMapPolygon.removeCoordinate(someCoordinate1)
            compare (preMapPolygonPathChanged.count, 3)
            preMapPolygon.removeCoordinate(someCoordinate1)
            compare (preMapPolygonPathChanged.count, 4)
            preMapPolygon.removeCoordinate(someCoordinate1)
            compare (preMapPolygonPathChanged.count, 4)

            // route
            preMapRoute.line.width = 5
            compare (preMapRouteLineWidthChanged.count, 1)
            preMapRoute.line.width = 5
            compare (preMapRouteLineWidthChanged.count, 1)
            preMapRoute.line.color = 'blue'
            compare (preMapRouteLineColorChanged.count, 1)
            preMapRoute.line.color = 'blue'
            compare (preMapRouteLineColorChanged.count, 1)
            preMapRoute.route = someRoute
            compare (preMapRouteRouteChanged.count, 1)
            preMapRoute.route = someRoute
            compare (preMapRouteRouteChanged.count, 1)

            // quick
            compare (preMapQuickItemCoordinateChanged.count, 0)
            preMapQuickItem.coordinate = someCoordinate1
            compare (preMapQuickItemCoordinateChanged.count, 1)
            preMapQuickItem.coordinate = someCoordinate1
            compare (preMapQuickItemCoordinateChanged.count, 1)
            preMapQuickItem.anchorPoint = Qt.point(39, 3)
            compare (preMapQuickItemAnchorPointChanged.count, 1)
            preMapQuickItem.anchorPoint = Qt.point(39, 3)
            compare (preMapQuickItemAnchorPointChanged.count, 1)
            preMapQuickItem.zoomLevel = 6
            compare (preMapQuickItemZoomLevelChanged.count, 1)
            preMapQuickItem.zoomLevel = 6
            compare (preMapQuickItemZoomLevelChanged.count, 1)
            preMapQuickItem.sourceItem = someItem
            compare (preMapQuickItemSourceItemChanged.count, 1)
            preMapQuickItem.sourceItem = someItem
            compare (preMapQuickItemSourceItemChanged.count, 1)
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

        // these 'real_' prefixed functions do sequences as
        // it would occur on real app (e.g. doubleclick is in fact
        // a sequence of press, release, doubleclick, release).
        // (they were recorded as seen on test app). mouseClick() works ok
        // because testlib internally converts it to mousePress + mouseRelease events
        function real_double_click (target, x, y) {
            mousePress(target, x,y)
            mouseRelease(target, x, y)
            mouseDoubleClick(target, x, y)
            mouseRelease(target, x, y)
        }
        function real_press_and_hold(target, x,y) {
            mousePress(target,x,y)
            wait(850) // threshold is 800 ms
            mouseRelease(target,x, y)
        }
    }
}
