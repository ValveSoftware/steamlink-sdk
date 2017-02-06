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
import QtLocation.Test 5.5

    /*

     (0,0)   ---------------------------------------------------- (296,0)
             | no map                                           |
             |    (20,20)                                       |
     (0,20)  |    ------------------------------------------    | (296,20)
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
     (0,296) ---------------------------------------------------- (296,296)

     */

Item {
    id: page
    x: 0; y: 0;
    width: 296
    height: 296
    Plugin { id: testPlugin; name : "qmlgeo.test.plugin"; allowExperimental: true }

    property variant mapDefaultCenter: QtPositioning.coordinate(20, 20)
    property variant preMapRectangleDefaultTopLeft: QtPositioning.coordinate(20, 20)
    property variant preMapRectangleDefaultBottomRight: QtPositioning.coordinate(10, 30)
    property variant preMapCircleDefaultCenter: QtPositioning.coordinate(10, 30)
    property variant preMapQuickItemDefaultCoordinate: QtPositioning.coordinate(35, 3)

    property variant preMapPolygonDefaultPath: [
        { latitude: 25, longitude: 5 },
        { latitude: 20, longitude: 10 },
        { latitude: 15, longitude: 6 }
    ]

    property variant preMapPolylineDefaultPath: [
        { latitude: 25, longitude: 15 },
        { latitude: 20, longitude: 19 },
        { latitude: 15, longitude: 16 }
    ]

    property variant preMapRouteDefaultPath: [
        { latitude: 25, longitude: 14 },
        { latitude: 20, longitude: 18 },
        { latitude: 15, longitude: 15 }
    ]

    property variant mapCircleTopLeft: QtPositioning.coordinate(0, 0)
    property variant mapCircleBottomRight: QtPositioning.coordinate(0, 0)
    property variant mapQuickItemTopLeft: QtPositioning.coordinate(0, 0)
    property variant mapQuickItemBottomRight: QtPositioning.coordinate(0, 0)
    property variant mapPolygonTopLeft: QtPositioning.coordinate(0, 0)
    property variant mapPolygonBottomRight: QtPositioning.coordinate(0, 0)
    property variant mapPolylineTopLeft: QtPositioning.coordinate(0, 0)
    property variant mapPolylineBottomRight: QtPositioning.coordinate(0, 0)
    property variant mapRouteTopLeft: QtPositioning.coordinate(0, 0)
    property variant mapRouteBottomRight: QtPositioning.coordinate(0, 0)
    property variant fitRect: QtPositioning.rectangle(QtPositioning.coordinate(80, 80),
                                                      QtPositioning.coordinate(78, 82))
    property variant fitEmptyRect: QtPositioning.rectangle(QtPositioning.coordinate(79, 79),-1, -1)
    property variant fitCircle: QtPositioning.circle(QtPositioning.coordinate(-50, -100), 1500)
    property variant fitInvalidShape: QtPositioning.shape()

    property variant fitCircleTopLeft: QtPositioning.coordinate(0, 0)
    property variant fitCircleBottomRight: QtPositioning.coordinate(0, 0)

    Map {
        id: map;
        x: 20; y: 20; width: 256; height: 256
        zoomLevel: 3
        center: mapDefaultCenter
        plugin: testPlugin;

        MapRectangle {
            id: preMapRect
            color: 'darkcyan'
            border.width: 0
            topLeft: preMapRectangleDefaultTopLeft
            bottomRight: preMapRectangleDefaultBottomRight
            MouseArea {
                id: preMapRectMa
                anchors.fill: parent
                drag.target: parent
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
            center: preMapCircleDefaultCenter
            radius: 400000
            MouseArea {
                anchors.fill: parent
                drag.target: parent
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
                anchors.fill: parent
                drag.target: parent
                SignalSpy { id: preMapQuickItemClicked; target: parent; signalName: "clicked" }
                SignalSpy { id: preMapQuickItemActiveChanged; target: parent.drag; signalName: "activeChanged" }
            }
            coordinate: preMapQuickItemDefaultCoordinate
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
        name: "MapItemsFitViewport"
        when: windowShown

        function initTestCase()
        {
            // sanity check that the coordinate conversion works
            var mapcenter = map.fromCoordinate(map.center)
            verify (fuzzy_compare(mapcenter.x, 128, 2))
            verify (fuzzy_compare(mapcenter.y, 128, 2))
        }

        function init()
        {
            preMapRect.topLeft.latitude = 20
            preMapRect.topLeft.longitude = 20
            preMapRect.bottomRight.latitude = 10
            preMapRect.bottomRight.longitude = 30
            preMapCircle.center.latitude = 10
            preMapCircle.center.longitude = 30
            preMapQuickItem.coordinate.latitude = 35
            preMapQuickItem.coordinate.longitude = 3
            var i
            for (i = 0; i < preMapPolygon.path.length; ++i) {
                preMapPolygon.path[i].latitude = preMapPolygonDefaultPath[i].latitude
                preMapPolygon.path[i].longitude = preMapPolygonDefaultPath[i].longitude
            }
            for (i = 0; i < preMapPolyline.path.length; ++i) {
                preMapPolyline.path[i].latitude = preMapPolylineDefaultPath[i].latitude
                preMapPolyline.path[i].longitude = preMapPolylineDefaultPath[i].longitude
            }
            for (i = 0; i < preMapRoute.route.path.length; ++i) {
                preMapRoute.route.path[i].latitude = preMapRouteDefaultPath[i].latitude
                preMapRoute.route.path[i].longitude = preMapRouteDefaultPath[i].longitude
            }
            // remove items
            map.clearMapItems()
            //clear_data()
            compare (map.mapItems.length, 0)
            // reset map
            map.center.latitude = 20
            map.center.longitude = 20
            map.zoomLevel = 3
            // re-add items and verify they are back (without needing to pan map etc.)
            map.addMapItem(preMapRect)
            map.addMapItem(preMapCircle)
            map.addMapItem(preMapQuickItem)
            map.addMapItem(preMapPolygon)
            map.addMapItem(preMapPolyline)
            map.addMapItem(preMapRoute)
            compare (map.mapItems.length, 6)
            calculate_bounds()
        }

        function test_visible_itmes()
        {
            // normal case - fit viewport to items which are all already visible
            verify_visibility_all_items()
            map.fitViewportToMapItems()
            visualInspectionPoint()
            verify_visibility_all_items()
        }

        function test_visible_zoom_in()
        {
            // zoom in (clipping also occurs)
            var z = map.zoomLevel
            for (var i = (z + 1); i < map.maximumZoomLevel; ++i ) {
                map.zoomLevel = i
                visualInspectionPoint()
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()
            }
        }

        function test_visible_zoom_out()
        {
            // zoom out
            for (var i = (z - 1); i >= 0; --i ) {
                map.zoomLevel = i
                visualInspectionPoint()
                verify_visibility_all_items()
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()
            }
        }

        function test_visible_map_move() {
            // move map so all items are out of screen
            // then fit viewport
            var xDir = 1
            var yDir = 0
            var xDirChange = -1
            var yDirChange = 1
            var dir = 0
            for (dir = 0; dir < 4; dir++) {
                verify_visibility_all_items()
                var i = 0
                var panX = map.width * xDir * 0.5
                var panY = map.height * yDir * 0.5
                map.pan(panX, panY)
                map.pan(panX, panY)
                visualInspectionPoint()
                // check all items are indeed not within screen bounds
                calculate_bounds()
                verify(!is_coord_on_screen(preMapRect.topLeft))
                verify(!is_coord_on_screen(preMapRect.bottomRight))
                verify(!is_coord_on_screen(mapCircleTopLeft))
                verify(!is_coord_on_screen(mapCircleBottomRight))
                verify(!is_coord_on_screen(mapPolygonTopLeft))
                verify(!is_coord_on_screen(mapPolygonBottomRight))
                verify(!is_coord_on_screen(mapQuickItemTopLeft))
                verify(!is_coord_on_screen(mapQuickItemBottomRight))
                verify(!is_coord_on_screen(mapPolylineTopLeft))
                verify(!is_coord_on_screen(mapPolylineBottomRight))
                verify(!is_coord_on_screen(mapRouteTopLeft))
                verify(!is_coord_on_screen(mapRouteBottomRight))
                // fit viewport and verify that all items are visible again
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()
                if (dir == 2)
                    xDirChange *= -1
                if (dir == 1)
                    yDirChange *= -1
                xDir += xDirChange
                yDir += yDirChange
            }
        }

        function test_fit_to_geoshape() {
            visualInspectionPoint()
            calculate_fit_circle_bounds()
            //None should be visible
            verify(!is_coord_on_screen(fitCircleTopLeft))
            verify(!is_coord_on_screen(fitCircleBottomRight))
            verify(!is_coord_on_screen(fitRect.topLeft))
            verify(!is_coord_on_screen(fitRect.bottomRight))

            map.visibleRegion = fitRect
            visualInspectionPoint()
            calculate_fit_circle_bounds()
            //Rectangle should be visible, not circle
            verify(!is_coord_on_screen(fitCircleTopLeft))
            verify(!is_coord_on_screen(fitCircleBottomRight))
            verify(is_coord_on_screen(fitRect.topLeft))
            verify(is_coord_on_screen(fitRect.bottomRight))

            map.visibleRegion = fitCircle
            visualInspectionPoint()
            calculate_fit_circle_bounds()
            //Circle should be visible, not rectangle
            verify(is_coord_on_screen(fitCircleTopLeft))
            verify(is_coord_on_screen(fitCircleBottomRight))
            verify(!is_coord_on_screen(fitRect.topLeft))
            verify(!is_coord_on_screen(fitRect.bottomRight))

            map.visibleRegion = fitInvalidShape
            visualInspectionPoint()
            calculate_fit_circle_bounds()
            //Invalid shape, map should be in the same position as before
            verify(is_coord_on_screen(fitCircleTopLeft))
            verify(is_coord_on_screen(fitCircleBottomRight))
            verify(!is_coord_on_screen(fitRect.topLeft))
            verify(!is_coord_on_screen(fitRect.bottomRight))

            map.visibleRegion = fitEmptyRect
            visualInspectionPoint()
            calculate_fit_circle_bounds()
            //Empty shape, map should change centerlocation, empty rect visible
            verify(!is_coord_on_screen(fitCircleTopLeft))
            verify(!is_coord_on_screen(fitCircleBottomRight))
            verify(is_coord_on_screen(fitEmptyRect.topLeft))
            verify(is_coord_on_screen(fitEmptyRect.bottomRight))

            // Test if this can be reset
            map.visibleRegion = fitRect
            verify(is_coord_on_screen(fitRect.topLeft))
            verify(is_coord_on_screen(fitRect.bottomRight))
            // move map
            map.center = QtPositioning.coordinate(0,0)
            verify(!is_coord_on_screen(fitRect.topLeft))
            verify(!is_coord_on_screen(fitRect.bottomRight))
            // recheck
            map.visibleRegion = fitRect
            verify(is_coord_on_screen(fitRect.topLeft))
            verify(is_coord_on_screen(fitRect.bottomRight))
            //zoom map
            map.zoomLevel++
            verify(!is_coord_on_screen(fitRect.topLeft))
            verify(!is_coord_on_screen(fitRect.bottomRight))
            // recheck
            map.visibleRegion = fitRect
            verify(is_coord_on_screen(fitRect.topLeft))
            verify(is_coord_on_screen(fitRect.bottomRight))
        }

        // checks that circles belongs to the view port
        function circle_in_viewport(center, radius, visible)
        {
            for (var i = 0; i < 128; ++i) {
                var azimuth = 360.0 * i / 128.0;
                var coord = center.atDistanceAndAzimuth(radius,azimuth)
                if (coord.isValid)
                    verify(is_coord_on_screen(coord) === visible, visible ?
                               "circle not visible" : "circle visible")
            }
        }

        function test_fit_circle_to_viewport(data)
        {
            verify(!is_coord_on_screen(data.center))
            circle_in_viewport(data.center, data.radius, false)
            map.visibleRegion = QtPositioning.circle(data.center, data.radius)
            circle_in_viewport(data.center, data.radius, true)
        }

        function test_fit_circle_to_viewport_data()
        {
            return [
                        { tag: "circle 1", center:
                            QtPositioning.coordinate(70,70), radius: 10 },
                        { tag: "circle 2", center:
                            QtPositioning.coordinate(80,30), radius: 2000000 },
                        { tag: "circle 3", center:
                            QtPositioning.coordinate(-82,30), radius: 2000000 },
                        { tag: "circle 4", center:
                            QtPositioning.coordinate(60,179), radius: 20000 },
                        { tag: "circle 5", center:
                            QtPositioning.coordinate(60,-179), radius: 20000 },
                    ]
        }

        function test_fit_rectangle_to_viewport(data)
        {
            verify(!is_coord_on_screen(data.topLeft),"rectangle visible")
            verify(!is_coord_on_screen(data.bottomRight),"rectangle visible")
            map.visibleRegion = QtPositioning.rectangle(data.topLeft,data.bottomRight)
            verify(is_coord_on_screen(data.topLeft),"rectangle not visible")
            verify(is_coord_on_screen(data.bottomRight),"rectangle not visible")
        }

        function test_fit_rectangle_to_viewport_data()
        {
            return [
                        { tag: "rectangle 1",
                            topLeft: QtPositioning.coordinate(80, 80),
                            bottomRight: QtPositioning.coordinate(78, 82) },
                        { tag: "rectangle 2",
                            topLeft: QtPositioning.coordinate(30,-130),
                            bottomRight: QtPositioning.coordinate(0,-100)}
                    ]
        }

        /*function test_ad_visible_items_move() {
            // move different individual items out of screen
            // then fit viewport
            var xDir = 1
            var yDir = 0
            var xDirChange = -1
            var yDirChange = 1
            var dir = 0
            var move = 50
            for (dir = 0; dir < 4; dir++) {
                // move rect out of screen
                reset()
                verify_visibility_all_items()
                preMapRect.topLeft.longitude += move * xDir
                preMapRect.topLeft.latitude += move * yDir
                preMapRect.bottomRight.longitude += move * xDir
                preMapRect.bottomRight.latitude += move * yDir
                calculate_bounds()
                verify(!is_coord_on_screen(preMapRect.topLeft))
                verify(!is_coord_on_screen(preMapRect.bottomRight))
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()

                // move circle out of screen
                reset()
                verify_visibility_all_items()
                preMapCircle.center.longitude += move * xDir
                preMapCircle.center.latitude += move * yDir
                calculate_bounds()
                verify(!is_coord_on_screen(mapCircleTopLeft))
                verify(!is_coord_on_screen(mapCircleBottomRight))
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()

                // move quick item out of screen
                reset()
                verify_visibility_all_items()
                preMapQuickItem.coordinate.longitude += move * xDir
                preMapQuickItem.coordinate.latitude += move * yDir
                calculate_bounds()
                verify(!is_coord_on_screen(mapQuickItemTopLeft))
                verify(!is_coord_on_screen(mapQuickItemBottomRight))
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()

                // move map polygon out of screen
                reset()
                verify_visibility_all_items()
                var i
                for (i = 0; i < preMapPolygonDefaultPath.length; ++i) {
                    preMapPolygon.path[i].longitude += move * xDir
                    preMapPolygon.path[i].latitude += move * yDir
                }
                calculate_bounds()
                verify(!is_coord_on_screen(mapPolygonTopLeft))
                verify(!is_coord_on_screen(mapPolygonBottomRight))
                map.fitViewportToMapItems()
                visualInspectionPoint()
                verify_visibility_all_items()
               if (dir == 2)
                    xDirChange *= -1
                if (dir == 1)
                    yDirChange *= -1
                xDir += xDirChange
                yDir += yDirChange
            }
        }*/

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

        function calculate_fit_circle_bounds() {
            var circleDiagonal = Math.sqrt(2 * fitCircle.radius * fitCircle.radius)
            fitCircleTopLeft = fitCircle.center.atDistanceAndAzimuth(circleDiagonal,-45)
            fitCircleBottomRight = fitCircle.center.atDistanceAndAzimuth(circleDiagonal,135)
        }

        function calculate_bounds(){
            var circleDiagonal = Math.sqrt(2 * preMapCircle.radius * preMapCircle.radius)
            var itemTopLeft = preMapCircle.center.atDistanceAndAzimuth(circleDiagonal,-45)
            var itemBottomRight = preMapCircle.center.atDistanceAndAzimuth(circleDiagonal,135)

            mapCircleTopLeft = itemTopLeft;
            mapCircleBottomRight = itemBottomRight;

            itemTopLeft = preMapQuickItem.coordinate
            var preMapQuickItemScreenPosition = map.fromCoordinate(preMapQuickItem.coordinate)
            preMapQuickItemScreenPosition.x += preMapQuickItem.sourceItem.width
            preMapQuickItemScreenPosition.y += preMapQuickItem.sourceItem.height
            itemBottomRight = map.toCoordinate(preMapQuickItemScreenPosition)

            mapQuickItemTopLeft = itemTopLeft;
            mapQuickItemBottomRight = itemBottomRight;

             var bounds = min_max_bounds_from_list(preMapPolygon.path)
             mapPolygonTopLeft = bounds.topLeft;
             mapPolygonBottomRight = bounds.bottomRight;

             bounds = min_max_bounds_from_list(preMapPolyline.path)
             mapPolylineTopLeft = bounds.topLeft;
             mapPolylineBottomRight = bounds.bottomRight;

             bounds = min_max_bounds_from_list(preMapRoute.route.path)
             mapRouteTopLeft = bounds.topLeft;
             mapRouteBottomRight = bounds.bottomRight;
        }

        function min_max_bounds_from_list(coorindates){
            var i = 0
            var point = map.fromCoordinate(coorindates[0])
            var minX = point.x
            var minY = point.y
            var maxX = point.x
            var maxY = point.y

            for (i=1; i < coorindates.length; ++i) {
                point = map.fromCoordinate(coorindates[i])
                if (point.x < minX)
                    minX = point.x
                if (point.x > maxX)
                    maxX = point.x
                if (point.y < minY)
                    minY = point.y
                if (point.y > maxY)
                    maxY = point.y
            }
            point.x = minX
            point.y = minY
            var itemTopLeft = map.toCoordinate(point)
            point.x = maxX
            point.y = maxY
            var itemBottomRight = map.toCoordinate(point)

            return QtPositioning.rectangle(itemTopLeft, itemBottomRight);
        }

        function verify_visibility_all_items(){
            calculate_bounds()
            verify(is_coord_on_screen(preMapRect.topLeft))
            verify(is_coord_on_screen(preMapRect.bottomRight))
            verify(is_coord_on_screen(mapCircleTopLeft))
            verify(is_coord_on_screen(mapCircleBottomRight))
            verify(is_coord_on_screen(mapPolygonTopLeft))
            verify(is_coord_on_screen(mapPolygonBottomRight))
            verify(is_coord_on_screen(mapQuickItemTopLeft))
            verify(is_coord_on_screen(mapQuickItemBottomRight))
            verify(is_coord_on_screen(mapPolylineTopLeft))
            verify(is_coord_on_screen(mapPolylineBottomRight))
            verify(is_coord_on_screen(mapRouteTopLeft))
            verify(is_coord_on_screen(mapRouteBottomRight))
        }


        function is_coord_on_screen(coord) {
            return is_point_on_screen(map.fromCoordinate(coord))
        }

        function is_point_on_screen(point) {
            if (point.x >= 0 && point.x <= (map.x + map.width)
                    && point.y >=0 && point.y <= (map.y + map.height) )
                return true;
            else
                return false;
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

