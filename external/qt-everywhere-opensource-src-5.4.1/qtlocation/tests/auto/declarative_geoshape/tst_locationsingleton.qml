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
import QtPositioning 5.2

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
}
