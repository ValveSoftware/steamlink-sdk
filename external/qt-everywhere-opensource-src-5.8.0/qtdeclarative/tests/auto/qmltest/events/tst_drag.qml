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
import QtTest 1.1

Rectangle{
    id: root
    width:200
    height:200

    TestUtil {
        id: util
    }

    SignalSpy {
        id: spyX
        target: container2
        signalName: "posXChanged"
    }
    SignalSpy {
        id: spyY
        target: container2
        signalName: "posYChanged"
    }

    Rectangle {
        id:container
        width:20
        height:20
        color: "blue"
        MouseArea {
            id:mouseArea; anchors.fill : parent
            drag.maximumX: 180
            drag.maximumY: 180
            drag.minimumX: 0
            drag.minimumY: 0
            drag.target: parent
        }
    }

    Rectangle {
        id: container2
        x: 25
        y: 25
        width:100
        height:100
        color: "red"
        property bool updatePositionWhileDragging: false
        property var posX: 0
        property var posY: 0

        function reset() {
            fakeHandle.x = 0
            fakeHandle.y = 0
            spyX.clear()
            spyY.clear()
        }

        Binding {
            when: container2.updatePositionWhileDragging
            target: container2
            property: "posX"
            value: fakeHandle.x
        }

        Binding {
            when: container2.updatePositionWhileDragging
            target: container2
            property: "posY"
            value: fakeHandle.y
        }

        Item { id: fakeHandle }

        MouseArea {
            anchors.fill : container2
            drag.maximumX: 180
            drag.maximumY: 180
            drag.minimumX: 0
            drag.minimumY: 0
            drag.target: fakeHandle

            onReleased: if (!container2.updatePositionWhileDragging) {
                            container2.posX = mouse.x;
                            container2.posY = mouse.y
                        }
        }
    }

    TestCase {
        name:"mouserelease"
        when:windowShown
        function test_mouseDrag() {
            mouseDrag(container, 10, 10, 20, 30);
            compare(container.x, 20 - util.dragThreshold - 1);
            compare(container.y, 30 - util.dragThreshold - 1);
        }

        function test_doSomethingWhileDragging() {
            container2.updatePositionWhileDragging = false
            // dx and dy are superior to 3 times util.dragThreshold.
            // but here the dragging does not update posX and posY
            // posX and posY are only updated on mouseRelease
            container2.reset()
            mouseDrag(container2, container2.x + 10, container2.y + 10, 10*util.dragThreshold, 10*util.dragThreshold);
            compare(spyX.count, 1)
            compare(spyY.count, 1)

            container2.updatePositionWhileDragging = true
            // dx and dy are superior to 3 times util.dragThreshold.
            // 3 intermediate mouseMove when dragging
            container2.reset()
            mouseDrag(container2, container2.x + 10, container2.y + 10, 10*util.dragThreshold, 10*util.dragThreshold);
            compare(spyX.count, 3)
            compare(spyY.count, 3)

            // dx and dy are inferior to 3 times util.dragThreshold.
            // No intermediate mouseMove when dragging, only one mouseMove
            container2.reset()
            mouseDrag(container2, container2.x + 10, container2.y + 10, 2*util.dragThreshold, 2*util.dragThreshold);
            compare(spyX.count, 1)
            compare(spyY.count, 1)

            // dx is superior to 3 times util.dragThreshold.
            // 3 intermediate mouseMove when dragging on x axis
            // no move on the y axis
            container2.reset()
            mouseDrag(container2, container2.x + 10, container2.y + 10, 10*util.dragThreshold, 0);
            compare(spyX.count, 3)
            compare(spyY.count, 0)

            // dy is inferior to 3 times util.dragThreshold.
            // No intermediate mouseMove when dragging, only one mouseMove on y axis
            // no move on the x axis
            container2.reset()
            mouseDrag(container2, container2.x + 10, container2.y + 10, 0, 2*util.dragThreshold);
            compare(spyX.count, 0)
            compare(spyY.count, 1)
        }
    }
}
