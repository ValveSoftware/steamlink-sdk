/****************************************************************************
**
** Copyright (C) 2016 Jeremy Katz
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
import QtQuick.Window 2.0
import QtTest 1.0

MultiPointTouchArea {
    id: touchArea
    width: 100
    height: 100

    SignalSpy {
        id: touchUpdatedSpy
        target: touchArea
        signalName: "touchUpdated"
    }

    SignalSpy {
        id: interiorSpy
        target: interior
        signalName: "touchUpdated"
    }

    MultiPointTouchArea {
        id: interior
        width: parent.width / 2
        height: parent.height
        anchors.right: parent.right
    }

    Window {
        width: 100; height: 100

        SignalSpy {
            id: subWindowSpy
            target: subWindowTouchArea
            signalName: "touchUpdated"
        }

        MultiPointTouchArea {
            id: subWindowTouchArea
            anchors.fill: parent
        }
    }

    TestCase {
        when: windowShown
        name: "touch"

        function comparePoint(point, id, x, y) {
            var retval = true;
            var pointId = point.pointId & 0xFFFFFF; //strip device identifier
            if (pointId !== id) {
                warn("Unexpected pointId: " + pointId + ". Expected " + id);
                retval = false;
            }
            if (point.x !== x) {
                warn("Unexpected x: " + point.x + ". Expected " + x);
                retval = false;
            }
            if (point.y !== y) {
                warn("Unexpected y: " + point.y + ". Expected " + y);
                retval = false;
            }
            return retval;
        }

        function cleanup() {
            touchUpdatedSpy.clear();
            interiorSpy.clear();
            subWindowSpy.clear();
        }

        function test_secondWindow() {
            var first = 1;
            var sequence = touchEvent(subWindowTouchArea);
            sequence.press(first, 0, 0, 0);
            sequence.commit();
            sequence.release(first, subWindowTouchArea, 0, 0)
            sequence.commit();
            compare(subWindowSpy.count, 2);
            var touchPoint = subWindowSpy.signalArguments[0][0][0];
            verify(comparePoint(touchPoint, first, 0, 0));
        }

        function initTestCase() {
            waitForRendering(touchArea) // when: windowShown may be insufficient
        }

        function test_childMapping() {
            var sequence = touchEvent(touchArea);

            var first = 1;
            // Test mapping touches to a child item
            sequence.press(first, interior, 0, 0);
            sequence.commit();

            // Map touches to the parent at the same point
            sequence.move(first, touchArea, interior.x, interior.y);
            sequence.commit();

            sequence.release(first, touchArea, interior.x, interior.y);
            sequence.commit();

            compare(interiorSpy.count, 3);
            verify(comparePoint(interiorSpy.signalArguments[0][0][0], first, 0, 0));
            verify(comparePoint(interiorSpy.signalArguments[1][0][0], first, 0, 0));
        }

        function test_fullSequence() {
            var sequence = touchEvent(touchArea);
            verify(sequence);

            var first = 1;
            var second = 2;

            sequence.press(first, null, 0, 0);
            sequence.commit();
            compare(touchUpdatedSpy.count, 1);
            var touchPoints = touchUpdatedSpy.signalArguments[0][0];
            compare(touchPoints.length, 1);
            verify(comparePoint(touchPoints[0], first, 0, 0));

            sequence.stationary(first);
            sequence.press(second, null, 1, 0);
            sequence.commit();
            compare(touchUpdatedSpy.count, 2);
            touchPoints = touchUpdatedSpy.signalArguments[1][0];
            compare(touchPoints.length, 2);
            verify(comparePoint(touchPoints[0], first, 0, 0));
            verify(comparePoint(touchPoints[1], second, 1, 0));

            sequence.release(first);
            sequence.move(second, null, 1, 1);
            sequence.commit();
            compare(touchUpdatedSpy.count, 3);
            touchPoints = touchUpdatedSpy.signalArguments[2][0];
            compare(touchPoints.length, 1);
            verify(comparePoint(touchPoints[0], second, 1, 1));

            sequence.release(second, null, 0, 1);
            sequence.commit();
            compare(touchUpdatedSpy.count, 4);
            touchPoints = touchUpdatedSpy.signalArguments[3][0];
            compare(touchPoints.length, 0);
        }

        function test_simpleChain() {
            var first = 1;
            touchEvent(touchArea).press(first).commit().release(first).commit();
            compare(touchUpdatedSpy.count, 2);
            var touchPoint =  touchUpdatedSpy.signalArguments[0][0][0];
            verify(comparePoint(touchPoint, first, touchArea.width / 2, touchArea.height / 2));
        }
    }
}
