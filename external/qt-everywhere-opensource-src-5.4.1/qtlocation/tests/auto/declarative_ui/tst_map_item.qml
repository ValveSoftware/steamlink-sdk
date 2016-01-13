/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import QtLocation.test 5.0
import QtPositioning 5.0

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

    property variant mapDefaultCenter: QtPositioning.coordinate(20, 20)
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
        zoomLevel: 3
        center: mapDefaultCenter
        plugin: testPlugin;

        MapRectangle {
            id: preMapRect
            color: 'darkcyan'
            border.width: 0
            topLeft {
                latitude: 20
                longitude: 20
            }
            bottomRight {
                latitude: 10
                longitude: 30
            }
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
            color: 'darkmagenta'
            border.width: 0
            center {
                latitude: 10
                longitude: 30
            }
            radius: 10000
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
            coordinate {
                latitude: 35
                longitude: 3
            }
            sourceItem: Rectangle {
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
        name: "Map Items"
        when: windowShown

        function test_aa_items_on_map() { // aa et al. for execution order
            wait(10)
            // sanity check that the coordinate conversion works, as
            // rest of the case relies on it. for robustness cut
            // a little slack with fuzzy compare
            var mapcenter = map.toScreenPosition(map.center)
            verify (fuzzy_compare(mapcenter.x, 100, 2))
            verify (fuzzy_compare(mapcenter.y, 100, 2))

            // precondition
            compare(preMapRectClicked.count, 0)
            compare(preMapCircleClicked.count, 0)

            // click rect
            map.center = preMapRect.topLeft
            var point = map.toScreenPosition(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapRectClicked.count, 1)
            mouseClick(map, 1, 1) // no item hit
            compare(preMapRectClicked.count, 1)
            compare(preMapCircleClicked.count, 0)

            // click circle, overlaps and is above rect
            map.center = preMapCircle.center
            point = map.toScreenPosition(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            compare(preMapRectClicked.count, 1)
            compare(preMapCircleClicked.count, 1)

            // click within circle bounding rect but not inside the circle geometry
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            compare(preMapRectClicked.count, 2)
            compare(preMapCircleClicked.count, 1)

            // click quick item
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            point = map.toScreenPosition(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapQuickItemClicked.count, 1)

            // click polygon
            compare (preMapPolygonClicked.count, 0)
            map.center = preMapPolygon.path[1]
            point = map.toScreenPosition(preMapPolygon.path[1])
            mouseClick(map, point.x - 5, point.y)
            compare(preMapPolygonClicked.count, 1)

            // remove items and repeat clicks to verify they are gone
            map.clearMapItems()
            clear_data()
            compare (map.mapItems.length, 0)
            map.center = preMapRect.topLeft
            point = map.toScreenPosition(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapRectClicked.count, 0)
            visualInspectionPoint()
            map.center = preMapCircle.center
            point = map.toScreenPosition(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            compare(preMapRectClicked.count, 0)
            compare(preMapCircleClicked.count, 0)
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            compare(preMapRectClicked.count, 0)
            compare(preMapCircleClicked.count, 0)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            point = map.toScreenPosition(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapPolygon.path[1]
            point = map.toScreenPosition(preMapPolygon.path[1])
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
            visualInspectionPoint()
            map.center = preMapRect.topLeft
            point = map.toScreenPosition(preMapRect.topLeft)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapRectClicked.count, 1)
            map.center = preMapCircle.center
            point = map.toScreenPosition(preMapCircle.center)
            mouseClick(map, point.x - 5, point.y - 5)
            compare(preMapRectClicked.count, 1)
            compare(preMapCircleClicked.count, 1)
            map.center = preMapCircle.center.atDistanceAndAzimuth(preMapCircle.radius, -45)
            mouseClick(map, preMapCircle.x + 4, preMapCircle.y + 4)
            compare(preMapRectClicked.count, 2)
            compare(preMapCircleClicked.count, 1)
            compare(preMapQuickItemClicked.count, 0)
            map.center = preMapQuickItem.coordinate
            point = map.toScreenPosition(preMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(preMapQuickItemClicked.count, 1)
            map.center = preMapPolygon.path[1]
            point = map.toScreenPosition(preMapPolygon.path[1])
            mouseClick(map, point.x - 5, point.y)
            compare(preMapPolygonClicked.count, 1)


            // item clips to map. not sure if this is sensible test
            map.addMapItem(extMapCircle)
            map.center = extMapCircle.center
            visualInspectionPoint();
            point = map.toScreenPosition(extMapCircle.center)
            mouseClick(map, point.x, point.y)
            compare(extMapCircleClicked.count, 1)
            mouseClick(map, point.x, -5)
            compare(extMapCircleClicked.count, 1)
            map.removeMapItem(extMapCircle)

            map.addMapItem(extMapQuickItem)
            map.center = extMapQuickItem.coordinate
            visualInspectionPoint();
            point = map.toScreenPosition(extMapQuickItem.coordinate)
            mouseClick(map, point.x + 5, point.y + 5)
            compare(extMapQuickItemClicked.count, 1)
            mouseClick(map, map.width + 5, point.y + 5)
            compare(extMapQuickItemClicked.count, 1)
            map.removeMapItem(extMapQuickItem)
        }

        function test_ab_drag() {
            clear_data()
            // basic drags, drag rectangle
            compare (preMapRectActiveChanged.count, 0)
            map.center = preMapRect.topLeft
            var i
            var point = map.toScreenPosition(preMapRect.topLeft)
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
            point = map.toScreenPosition(preMapCircle.center)
            targetCoordinate = map.toCoordinate(51, 51)
            mousePress(map, point.x, point.y)
            for (i = 0; i < 50; i += 1) {
                wait(1)
                mouseMove(map, point.x - i, point.y - i)
            }
            mouseRelease(map, point.x - i, point.y - i)
            visualInspectionPoint()
            compare(preMapRectActiveChanged.count, 2)
            compare(preMapCircleActiveChanged.count, 2)
            verify(preMapCircleCenterChanged.count > 1)
            verify(fuzzy_compare(preMapCircle.center.latitude, targetCoordinate.latitude, 0.2))
            verify(fuzzy_compare(preMapCircle.center.longitude, targetCoordinate.longitude, 0.2))

            // drag quick item
            compare (preMapQuickItemActiveChanged.count, 0)
            map.center = preMapQuickItem.coordinate
            point = map.toScreenPosition(preMapQuickItem.coordinate)
            targetCoordinate = map.toCoordinate(51, 51)
            mousePress(map, point.x + 5, point.y + 5)
            for (i = 0; i < 50; i += 1) {
                wait(1)
                mouseMove(map, point.x - i, point.y - i)
            }
            mouseRelease(map, point.x - i, point.y - i)
            visualInspectionPoint()
            compare(preMapQuickItemActiveChanged.count, 2)
            verify(preMapQuickItemCoordinateChanged.count > 1)
            verify(fuzzy_compare(preMapQuickItem.coordinate.latitude, targetCoordinate.latitude, 0.2))
            verify(fuzzy_compare(preMapQuickItem.coordinate.longitude, targetCoordinate.longitude, 0.2))
        }

        function test_ac_basic_properties() {
            clear_data()
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

        function clear_data() {
            preMapRectClicked.clear()
            preMapCircleClicked.clear()
            preMapQuickItemClicked.clear()
            preMapPolygonClicked.clear()
            preMapCircleCenterChanged.clear()
            preMapCircleColorChanged.clear()
            preMapCircleRadiusChanged.clear()
            preMapCircleBorderColorChanged.clear()
            preMapCircleBorderWidthChanged.clear()
            preMapRectTopLeftChanged.clear()
            preMapRectBottomRightChanged.clear()
            preMapRectColorChanged.clear()
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
            preMapQuickItemCoordinateChanged.clear()
            preMapQuickItemAnchorPointChanged.clear()
            preMapQuickItemZoomLevelChanged.clear()
            preMapQuickItemSourceItemChanged.clear()
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
