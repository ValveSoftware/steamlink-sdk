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
import QtLocation 5.5

Item {
    id: testCase

    property variant coordinate1: QtPositioning.coordinate(1, 1)
    property variant coordinate2: QtPositioning.coordinate(2, 2)
    property variant coordinate3: QtPositioning.coordinate(80, 80)

    property variant emptyCircle: QtPositioning.circle()
    property variant circle1: QtPositioning.circle(coordinate1, 200000)

    SignalSpy { id: circleChangedSpy; target: testCase; signalName: "emptyCircleChanged" }

    TestCase {
        name: "Bounding circle"
        function test_circle_defaults_and_setters() {
            circleChangedSpy.clear();
            compare (emptyCircle.radius, -1)
            compare (circle1.radius, 200000)

            emptyCircle.radius = 200
            compare(circleChangedSpy.count, 1);
            emptyCircle.radius = 200;
            compare(circleChangedSpy.count, 1);

            emptyCircle.center = coordinate1;
            compare(circleChangedSpy.count, 2);
            emptyCircle.center = coordinate1
            compare(circleChangedSpy.count, 2);
            emptyCircle.center = coordinate2
            compare(circleChangedSpy.count, 3);

            emptyCircle.center = coordinate1
            emptyCircle.radius = 200000

            compare(emptyCircle.contains(coordinate1), true);
            compare(emptyCircle.contains(coordinate2), true);
            compare(emptyCircle.contains(coordinate3), false);
        }
    }

    // coordinate unit square
    property variant bl: QtPositioning.coordinate(0, 0)
    property variant tl: QtPositioning.coordinate(1, 0)
    property variant tr: QtPositioning.coordinate(1, 1)
    property variant br: QtPositioning.coordinate(0, 1)
    property variant ntr: QtPositioning.coordinate(3, 3)

    property variant invalid: QtPositioning.coordinate(100, 190)
    property variant inside: QtPositioning.coordinate(0.5, 0.5)
    property variant outside: QtPositioning.coordinate(2, 2)

    property variant box: QtPositioning.rectangle(tl, br)

    property variant coordinates: [bl, tl, tr, br]
    property variant coordinates2: [bl, tl, tr, br, ntr]
    property variant coordinates3: [tr]
    property variant coordinates4: [invalid]
    property variant coordinates5: []

    property variant listBox: QtPositioning.rectangle(coordinates)
    property variant listBox2: QtPositioning.rectangle(coordinates2)
    property variant listBox3: QtPositioning.rectangle(coordinates3)
    property variant listBox4: QtPositioning.rectangle(coordinates4)
    property variant listBox5: QtPositioning.rectangle(coordinates5)

    property variant widthBox: QtPositioning.rectangle(inside, 1, 1);

    // C++ auto test exists for basics of bounding box, testing here
    // only added functionality
    TestCase {
        name: "Bounding box"
        function test_box_defaults_and_setters() {
            compare (box.bottomRight.longitude, br.longitude) // sanity
            compare (box.contains(bl), true)
            compare (box.contains(inside), true)
            compare (box.contains(outside), false)
            box.topRight = ntr
            compare (box.contains(outside), true)

            compare (listBox.isValid, true)
            compare (listBox.contains(outside), false)
            compare (listBox2.contains(outside), true)
            compare (listBox3.isValid, true)
            compare (listBox3.isEmpty, true)
            compare (listBox4.isValid, false)
            compare (listBox5.isValid, false)

            compare (widthBox.contains(inside), true)
            compare (widthBox.contains(outside), false)
        }
    }

    TestCase {
        name: "Shape"

        function test_shape_comparison_data() {
            return [
                { tag: "invalid shape", shape1: QtPositioning.shape(), shape2: QtPositioning.shape(), result: true },
                { tag: "box equal", shape1: box, shape2: QtPositioning.rectangle(tl, br), result: true },
                { tag: "box not equal", shape1: box, shape2: QtPositioning.rectangle([inside, outside]), result: false },
                { tag: "box invalid shape", rect1: box, shape2: QtPositioning.shape(), result: false },
                { tag: "invalid rectangle", shape1: QtPositioning.rectangle(), shape2: QtPositioning.rectangle(), result: true },
                { tag: "invalid rectangle2", shape1: QtPositioning.rectangle(), shape2: QtPositioning.shape(), result: false },
                { tag: "circle1 equal", shape1: circle1, shape2: QtPositioning.circle(coordinate1, 200000), result: true },
                { tag: "circle1 not equal", shape1: circle1, shape2: QtPositioning.circle(coordinate2, 2000), result: false },
                { tag: "circle1 invalid shape", shape1: circle1, shape2: QtPositioning.shape(), result: false },
                { tag: "invalid circle", shape1: QtPositioning.circle(), shape2: QtPositioning.circle(), result: true },
                { tag: "invalid circle2", shape1: QtPositioning.circle(), shape2: QtPositioning.shape(), result: false }
            ]
        }

        function test_shape_comparison(data) {
            compare(data.shape1 === data.shape2, data.result)
            compare(data.shape1 !== data.shape2, !data.result)
            compare(data.shape1 == data.shape2, data.result)
            compare(data.shape1 != data.shape2, !data.result)
        }
    }

    TestCase {
        name: "Conversions"

        function test_shape_circle_conversions() {
            var circle = QtPositioning.shapeToCircle(QtPositioning.shape())
            verify(!circle.isValid)
            circle = QtPositioning.shapeToCircle(QtPositioning.circle())
            verify(!circle.isValid)
            circle = QtPositioning.shapeToCircle(QtPositioning.circle(tl, 10000))
            verify(circle.isValid)
            compare(circle.center, tl)
            compare(circle.radius, 10000)
            circle = QtPositioning.shapeToCircle(QtPositioning.rectangle())
            verify(!circle.isValid)
            circle = QtPositioning.shapeToCircle(QtPositioning.rectangle(tl, br))
            verify(!circle.isValid)
            circle = QtPositioning.shapeToCircle(listBox)
            verify(!circle.isValid)
        }

        function test_shape_rectangle_conversions() {
            var rectangle = QtPositioning.shapeToRectangle(QtPositioning.shape())
            verify(!rectangle.isValid)
            rectangle = QtPositioning.shapeToRectangle(QtPositioning.circle())
            verify(!rectangle.isValid)
            rectangle = QtPositioning.shapeToRectangle(QtPositioning.circle(tl, 10000))
            verify(!rectangle.isValid)
            rectangle = QtPositioning.shapeToRectangle(QtPositioning.rectangle())
            verify(!rectangle.isValid)
            rectangle = QtPositioning.shapeToRectangle(QtPositioning.rectangle(tl, br))
            verify(rectangle.isValid)
            compare(rectangle.topLeft, tl)
            compare(rectangle.bottomRight, br)
            rectangle = QtPositioning.shapeToRectangle(listBox)
            verify(rectangle.isValid)
        }
    }


    MapPolyline {
        id: mapPolyline
        path: [
                { latitude: -27, longitude: 153.0 },
                { latitude: -27, longitude: 154.1 },
                { latitude: -28, longitude: 153.5 },
                { latitude: -29, longitude: 153.5 }
            ]
    }

    TestCase {
        name: "MapPolyline path"
        function test_path_operations() {
            compare(mapPolyline.path[1].latitude, -27)
            compare(mapPolyline.path[1].longitude, 154.1)
            compare(mapPolyline.coordinateAt(1), QtPositioning.coordinate(-27, 154.1))
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.removeCoordinate(1);
            compare(mapPolyline.path[1].latitude, -28)
            compare(mapPolyline.path[1].longitude, 153.5)
            compare(mapPolyline.coordinateAt(1), QtPositioning.coordinate(-28, 153.5))
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.addCoordinate(QtPositioning.coordinate(30, 153.1))
            compare(mapPolyline.path[mapPolyline.path.length-1].latitude, 30)
            compare(mapPolyline.path[mapPolyline.path.length-1].longitude, 153.1)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(30, 153.1)), true)
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.removeCoordinate(QtPositioning.coordinate(30, 153.1))
            compare(mapPolyline.path[mapPolyline.path.length-1].latitude, -29)
            compare(mapPolyline.path[mapPolyline.path.length-1].longitude, 153.5)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(30, 153.1)), false)
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.insertCoordinate(2, QtPositioning.coordinate(35, 153.1))
            compare(mapPolyline.path[2].latitude, 35)
            compare(mapPolyline.path[2].longitude, 153.1)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(35, 153.1)), true)
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.replaceCoordinate(2, QtPositioning.coordinate(45, 150.1))
            compare(mapPolyline.path[2].latitude, 45)
            compare(mapPolyline.path[2].longitude, 150.1)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(35, 153.1)), false)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(45, 150.1)), true)
            compare(mapPolyline.path.length, mapPolyline.pathLength())

            mapPolyline.insertCoordinate(2, QtPositioning.coordinate(35, 153.1))
            compare(mapPolyline.coordinateAt(2).latitude, 35)
            compare(mapPolyline.coordinateAt(2).longitude, 153.1)
            compare(mapPolyline.containsCoordinate(QtPositioning.coordinate(35, 153.1)), true)
            compare(mapPolyline.path.length, mapPolyline.pathLength())
        }
    }
}
