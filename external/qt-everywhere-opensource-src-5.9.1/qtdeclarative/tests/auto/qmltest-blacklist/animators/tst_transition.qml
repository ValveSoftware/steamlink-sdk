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
    width: 200
    height: 200

    TestCase {
        id: testcase
        name: "animators-transition"
        when: box.scale == 2
        function test_endresult() {
            compare(box.scaleChangeCounter, 1);
            compare(box.scale, 2);
            var image = grabImage(root);
            verify(image.pixel(0, 0) == Qt.rgba(1, 0, 0));
            verify(image.pixel(199, 199) == Qt.rgba(0, 0, 1));
        }
    }

    states: [
        State {
            name: "one"
            PropertyChanges { target: box; scale: 1 }
        },
        State {
            name: "two"
            PropertyChanges { target: box; scale: 2 }
        }
    ]
    state: "one"

    transitions: [
        Transition {
            ScaleAnimator { duration: 100; }
        }
    ]

    Box {
        id: box
    }

    Timer {
        interval: 100;
        repeat: false
        running: true
        onTriggered: root.state = "two"
    }
}
