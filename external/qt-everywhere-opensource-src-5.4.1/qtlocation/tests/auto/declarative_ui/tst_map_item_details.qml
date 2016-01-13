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
import QtPositioning 5.0
import QtLocation 5.3
import QtLocation.test 5.0

Item {
    id: page
    x: 0; y: 0;
    width: 240
    height: 240
    Plugin { id: testPlugin; name : "qmlgeo.test.plugin"; allowExperimental: true }

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
        zoomLevel: 3
        center: mapDefaultCenter
        plugin: testPlugin;
    }

    Text {id: progressText}

    TestCase {
        name: "Map Item 2"
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
        function test_aa_precondition() {
            wait(10)
            // sanity check that the coordinate conversion works
            var mapcenter = map.toScreenPosition(map.center)
            verify (fuzzy_compare(mapcenter.x, 100, 2))
            verify (fuzzy_compare(mapcenter.y, 100, 2))
        }

        function test_polygon() {
            map.clearMapItems()
            clear_data()
            compare (extMapPolygon.border.width, 1.0)
            compare (extMapPolygonClicked.count, 0)
            map.center = extMapPolygon.path[1]
            var point = map.toScreenPosition(extMapPolygon.path[1])
            map.addMapItem(extMapPolygon)
            verify(extMapPolygon.path.length == 2)
            mouseClick(map, point.x - 5, point.y)
            compare(extMapPolygonClicked.count, 0)
            map.addMapItem(extMapPolygon0) // mustn't crash or ill-behave
            verify(extMapPolygon0.path.length == 0)
            extMapPolygon.addCoordinate(polyCoordinate)
            verify(extMapPolygon.path.length == 3)
            mouseClick(map, point.x - 5, point.y)
            compare(extMapPolygonClicked.count, 1)

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

        function test_polyline() {
            map.clearMapItems()
            clear_data()
            compare (extMapPolyline.line.width, 1.0)
            var point = map.toScreenPosition(extMapPolyline.path[1])
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
        function test_yz_dateline() {
            map.clearMapItems()
            clear_data()
            map.center = datelineCoordinate
            map.zoomLevel = 2.2

            // rectangle
            // item spanning across dateline
            map.addMapItem(extMapRectDateline)
            verify(extMapRectDateline.topLeft.longitude == 175)
            verify(extMapRectDateline.bottomRight.longitude == -175)
            var point = map.toScreenPosition(extMapRectDateline.topLeft)
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            // move item away from dataline by directly setting its coords
            extMapRectDateline.bottomRight.longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            // move item edge onto dateline
            extMapRectDateline.topLeft.longitude = datelineCoordinate.longitude
            point = map.toScreenPosition(extMapRectDateline.topLeft)
            verify(point.x == map.width / 2.0)
            // drag item back onto dateline
            mousePress(map, point.x + 5, point.y + 5)
            var i
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y + 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y + 5)
            point = map.toScreenPosition(extMapRectDateline.topLeft)
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapRectDateline.bottomRight)
            verify(point.x > map.width / 2.0)
            map.removeMapItem(extMapRectDateline)

            // circle
            map.addMapItem(extMapCircleDateline)
            verify(extMapCircleDateline.center.longitude === 180)
            map.center = datelineCoordinate
            point = map.toScreenPosition(extMapCircleDateline.center)
            verify(point.x == map.width / 2.0)
            extMapCircleDateline.center.longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapCircleDateline.center)
            verify(point.x > map.width / 2.0)
            mousePress(map, point.x, point.y)
            for (i=0; i < 40; i += 4) {
                wait(1)
                mouseMove(map, point.x - i, point.y)
            }
            mouseRelease(map, point.x - i, point.y)
            point = map.toScreenPosition(extMapCircleDateline.center)
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapCircleDateline)

            // quickitem
            map.addMapItem(extMapQuickItemDateline)
            map.center = datelineCoordinate
            verify(extMapQuickItemDateline.coordinate.longitude === 175)
            point = map.toScreenPosition(extMapQuickItemDateline.coordinate)
            verify(point.x < map.width / 2.0)
            extMapQuickItemDateline.coordinate.longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapQuickItemDateline.coordinate)
            verify(point.x > map.width / 2.0)
            mousePress(map, point.x + 5, point.y + 5)
            for (i=0; i < 50; i += 5) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y + 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y + 5)
            point = map.toScreenPosition(extMapQuickItemDateline.coordinate)
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapQuickItemDateline)

            // polygon
            map.addMapItem(extMapPolygonDateline)
            map.center = datelineCoordinate
            verify(extMapPolygonDateline.path[0].longitude == 175)
            verify(extMapPolygonDateline.path[1].longitude == -175)
            verify(extMapPolygonDateline.path[2].longitude == -175)
            verify(extMapPolygonDateline.path[3].longitude == 175)
            point = map.toScreenPosition(extMapPolygonDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[3])
            verify(point.x < map.width / 2.0)
            extMapPolygonDateline.path[1].longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            extMapPolygonDateline.path[2].longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            var path = extMapPolygonDateline.path;
            path[0].longitude = datelineCoordinate.longitude;
            extMapPolygonDateline.path = path;
            point = map.toScreenPosition(extMapPolygonDateline.path[0])
            verify(point.x == map.width / 2.0)
            path = extMapPolygonDateline.path;
            path[3].longitude = datelineCoordinate.longitude;
            extMapPolygonDateline.path = path;
            point = map.toScreenPosition(extMapPolygonDateline.path[3])
            verify(point.x == map.width / 2.0)
            mousePress(map, point.x + 5, point.y - 5)
            for (i=0; i < 16; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 - i, point.y - 5 )
            }
            mouseRelease(map, point.x + 5 - i, point.y - 5)
            point = map.toScreenPosition(extMapPolygonDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[1])
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[2])
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonDateline.path[3])
            verify(point.x < map.width / 2.0)
            map.removeMapItem(extMapPolygonDateline)

            // polyline
            map.addMapItem(extMapPolylineDateline)
            map.center = datelineCoordinate
            verify(extMapPolylineDateline.path[0].longitude == 175)
            verify(extMapPolylineDateline.path[1].longitude == -175)
            point = map.toScreenPosition(extMapPolylineDateline.path[0])
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapPolylineDateline.path[1])
            verify(point.x > map.width / 2.0)
            extMapPolylineDateline.path[1].longitude = datelineCoordinateRight.longitude
            point = map.toScreenPosition(extMapPolylineDateline.path[1])
            verify(point.x > map.width / 2.0)
            var path = extMapPolygonDateline.path;
            path[0].longitude = datelineCoordinate.longitude;
            extMapPolylineDateline.path = path;
            point = map.toScreenPosition(extMapPolylineDateline.path[0])
            verify(point.x == map.width / 2.0)
            map.removeMapItem(extMapPolylineDateline)

            // map route
            // (does not support setting of path coords)
            map.addMapItem(extMapRouteDateline)
            verify(extMapRouteDateline.route.path[0].longitude == 175)
            verify(extMapRouteDateline.route.path[1].longitude == -175)
            point = map.toScreenPosition(extMapRouteDateline.route.path[0])
            verify(point.x < map.width / 2.0)
            point = map.toScreenPosition(extMapRouteDateline.route.path[1])
            verify(point.x > map.width / 2.0)
            map.removeMapItem(extMapRouteDateline)
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
        function test_zz_border_drag() {
            map.clearMapItems()
            clear_data()
            map.center = datelineCoordinate

            // lower zoom level and change widths to reveal map border.
            // Note: items are not visible at zoom level < 2.0,
            // but for testing it's ok
            map.zoomLevel = 1
            page.width = 600
            map.width = 560

            // rectangle
            map.addMapItem(extMapRectEdge)
            verify(extMapRectEdge.topLeft.longitude == -15)
            verify(extMapRectEdge.bottomRight.longitude == -5)
            var point = map.toScreenPosition(extMapRectEdge.topLeft)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapRectEdge.bottomRight)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            var originalWidth = extMapRectEdge.width;
            verify(originalWidth < map.width / 2.0)
            // drag item onto map border
            point = map.toScreenPosition(extMapRectEdge.topLeft)
            mousePress(map, point.x + 5, point.y + 5)
            var i
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 + i, point.y + 5)
            }
            mouseRelease(map, point.x + 5 + i, point.y + 5)
            // currently the bottom right screen point is unwrapped and drawn
            // out of the map border, but in the future culling may take place
            // so tests for points outside the map border are ignored,
            // instead we check the item width
            point = map.toScreenPosition(extMapRectEdge.topLeft)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            compare(extMapRectEdge.width, originalWidth)
            map.removeMapItem(extMapRectEdge)

            // circle
            map.addMapItem(extMapCircleEdge)
            map.center = datelineCoordinate
            verify(extMapCircleEdge.center.longitude == -15)
            var point = map.toScreenPosition(extMapCircleEdge.center)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            originalWidth = extMapCircleEdge.width;
            verify(originalWidth < map.width / 2.0)
            point = map.toScreenPosition(extMapCircleEdge.center)
            mousePress(map, point.x, point.y)
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + i, point.y)
            }
            mouseRelease(map, point.x + i, point.y)
            point = map.toScreenPosition(extMapCircleEdge.center)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            fuzzy_compare(extMapCircleEdge.width, originalWidth)
            map.removeMapItem(extMapCircleEdge)

            // quickitem
            map.addMapItem(extMapQuickItemEdge)
            map.center = datelineCoordinate
            verify(extMapQuickItemEdge.coordinate.longitude == -15)
            point = map.toScreenPosition(extMapQuickItemEdge.coordinate)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            originalWidth = extMapQuickItemEdge.width;
            verify(originalWidth < map.width / 2.0)
            mousePress(map, point.x + 5, point.y + 5)
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 + i, point.y + 5)
            }
            mouseRelease(map, point.x + 5 + i, point.y + 5)
            point = map.toScreenPosition(extMapQuickItemEdge.coordinate)
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            compare(extMapQuickItemEdge.width, originalWidth)
            map.removeMapItem(extMapQuickItemEdge)

            // polygon
            map.center = datelineCoordinate
            map.addMapItem(extMapPolygonEdge)
            verify(extMapPolygonEdge.path[0].longitude == -15)
            verify(extMapPolygonEdge.path[1].longitude == -5)
            verify(extMapPolygonEdge.path[2].longitude == -5)
            verify(extMapPolygonEdge.path[3].longitude == -15)
            point = map.toScreenPosition(extMapPolygonEdge.path[0])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonEdge.path[1])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonEdge.path[2])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonEdge.path[3])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            originalWidth = extMapPolygonEdge.width;
            verify(originalWidth < map.width / 2.0)
            mousePress(map, point.x + 5, point.y - 5)
            for (i=0; i < 20; i += 2) {
                wait(1)
                mouseMove(map, point.x + 5 + i, point.y - 5)
            }
            mouseRelease(map, point.x + 5 + i, point.y - 5)
            point = map.toScreenPosition(extMapPolygonEdge.path[0])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            point = map.toScreenPosition(extMapPolygonEdge.path[3])
            verify(point.x < map.width)
            verify(point.x > map.width / 2.0)
            compare(extMapPolygonEdge.width, originalWidth)
            map.removeMapItem(extMapPolygonEdge)
        }


        function clear_data() {
            extMapPolygonClicked.clear()
            extMapPolylineColorChanged.clear()
            extMapPolylineWidthChanged.clear()
            extMapPolylinePathChanged.clear()
            extMapPolygonPathChanged.clear()
            extMapPolygonColorChanged.clear()
            extMapPolygonBorderColorChanged.clear()
            extMapPolygonBorderWidthChanged.clear()
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

        // call to visualInspectionPoint testcase (only for development time visual inspection)
        function visualInspectionPoint(text) {
            progressText.text = text
            //wait (500)
        }
    }
}
