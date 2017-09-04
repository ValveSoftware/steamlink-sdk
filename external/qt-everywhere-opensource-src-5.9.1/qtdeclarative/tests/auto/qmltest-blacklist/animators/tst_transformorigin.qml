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

import QtQuick 2.2
import QtTest 1.1

Item {
    id: root;
    width: 300
    height: 300

    Timer {
        id: timer;
        running: testCase.windowShown
        interval: 1000
        repeat: false
        onTriggered: triggered = true;
        property bool triggered: false;
    }

    TestCase {
        id: testCase
        name: "animators-transformorigin"
        when: timer.triggered
        function test_endresult() {

            var image = grabImage(root);

            var white = Qt.rgba(1, 1, 1);
            var blue = Qt.rgba(0, 0, 1);


            // topleft
            verify(image.pixel(40, 40) == white);
            verify(image.pixel(60, 40) == white);
            verify(image.pixel(40, 60) == white);
            verify(image.pixel(60, 60) == blue);

            // top
            verify(image.pixel(140, 40) == white);
            verify(image.pixel(160, 40) == white);
            verify(image.pixel(140, 60) == blue);
            verify(image.pixel(160, 60) == blue);

            // topright
            verify(image.pixel(240, 40) == white);
            verify(image.pixel(260, 40) == white);
            verify(image.pixel(240, 60) == blue);
            verify(image.pixel(260, 60) == white);


            // left
            verify(image.pixel(40, 140) == white);
            verify(image.pixel(60, 140) == blue);
            verify(image.pixel(40, 160) == white);
            verify(image.pixel(60, 160) == blue);

            // center
            verify(image.pixel(140, 140) == blue);
            verify(image.pixel(160, 140) == blue);
            verify(image.pixel(140, 160) == blue);
            verify(image.pixel(160, 160) == blue);

            // right
            verify(image.pixel(240, 140) == blue);
            verify(image.pixel(260, 140) == white);
            verify(image.pixel(240, 160) == blue);
            verify(image.pixel(260, 160) == white);


            // bottomleft
            verify(image.pixel(40, 240) == white);
            verify(image.pixel(60, 240) == blue);
            verify(image.pixel(40, 260) == white);
            verify(image.pixel(60, 260) == white);

            // bottom
            verify(image.pixel(140, 240) == blue);
            verify(image.pixel(160, 240) == blue);
            verify(image.pixel(140, 260) == white);
            verify(image.pixel(160, 260) == white);

            // bottomright
            verify(image.pixel(240, 240) == blue);
            verify(image.pixel(260, 240) == white);
            verify(image.pixel(240, 260) == white);
            verify(image.pixel(260, 260) == white);

        }
    }

    property var origins: [Item.TopLeft, Item.Top, Item.TopRight,
                           Item.Left, Item.Center, Item.Right,
                           Item.BottomLeft, Item.Bottom, Item.BottomRight];

    Grid {
        anchors.fill: parent
        rows: 3
        columns: 3

        Repeater {
            model: 9
            Item {
                width: 100
                height: 100
                Rectangle {
                    id: box
                    color: "blue"
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    antialiasing: true;

                    transformOrigin: root.origins[index];

                    ScaleAnimator { target: box; from: 1; to: 5.5; duration: 100; running: true; }
                }
            }
        }
    }

}
