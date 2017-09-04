/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Controls 2.2

TestCase {
    id: testCase
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "Dial"

    Component {
        id: dialComponent
        Dial {}
    }

    Component {
        id: signalSpy
        SignalSpy {}
    }

    function test_instance() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);
        compare(dial.value, 0.0);
        compare(dial.from, 0.0);
        compare(dial.to, 1.0);
        compare(dial.stepSize, 0.0);
        verify(dial.activeFocusOnTab);
        verify(!dial.pressed);
    }

    function test_value() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);
        compare(dial.value, 0.0);

        dial.value = 0.5;
        compare(dial.value, 0.5);

        dial.value = 1.0;
        compare(dial.value, 1.0);

        dial.value = -1.0;
        compare(dial.value, 0.0);

        dial.value = 2.0;
        compare(dial.value, 1.0);
    }

    function test_range() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        dial.from = 0;
        dial.to = 100;
        dial.value = 50;
        compare(dial.from, 0);
        compare(dial.to, 100);
        compare(dial.value, 50);
        compare(dial.position, 0.5);

        dial.value = 1000
        compare(dial.value, 100);
        compare(dial.position, 1);

        dial.value = -1
        compare(dial.value, 0);
        compare(dial.position, 0);

        dial.from = 25
        compare(dial.from, 25);
        compare(dial.value, 25);
        compare(dial.position, 0);

        dial.to = 75
        compare(dial.to, 75);
        compare(dial.value, 25);
        compare(dial.position, 0);

        dial.value = 50
        compare(dial.value, 50);
        compare(dial.position, 0.5);
    }

    function test_inverted() {
        var dial = createTemporaryObject(dialComponent, testCase, { from: 1.0, to: -1.0 });
        verify(dial);
        compare(dial.from, 1.0);
        compare(dial.to, -1.0);
        compare(dial.value, 0.0);
        compare(dial.position, 0.5);

        dial.value = 2.0;
        compare(dial.value, 1.0);
        compare(dial.position, 0.0);

        dial.value = -2.0;
        compare(dial.value, -1.0);
        compare(dial.position, 1.0);

        dial.value = 0.0;
        compare(dial.value, 0.0);
        compare(dial.position, 0.5);
    }

    SignalSpy {
        id: pressSpy
        signalName: "pressedChanged"
    }

    function test_pressed() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        pressSpy.target = dial;
        verify(pressSpy.valid);
        verify(!dial.pressed);

        mousePress(dial, dial.width / 2, dial.height / 2);
        verify(dial.pressed);
        compare(pressSpy.count, 1);

        mouseRelease(dial, dial.width / 2, dial.height / 2);
        verify(!dial.pressed);
        compare(pressSpy.count, 2);

        var touch = touchEvent(dial);
        touch.press(0).commit();
        verify(dial.pressed);
        compare(pressSpy.count, 3);

        touch.release(0).commit();
        verify(!dial.pressed);
        compare(pressSpy.count, 4);
    }

    SignalSpy {
        id: valueSpy
        signalName: "valueChanged"
    }

    function test_dragging_data() {
        return [
            { tag: "default", from: 0, to: 1, leftValue: 0.20, topValue: 0.5, rightValue: 0.8, bottomValue: 1.0, live: false },
            { tag: "scaled2", from: 0, to: 2, leftValue: 0.4, topValue: 1.0, rightValue: 1.6, bottomValue: 2.0, live: false },
            { tag: "scaled1", from: -1, to: 0, leftValue: -0.8, topValue: -0.5, rightValue: -0.2, bottomValue: 0.0, live: false },
            { tag: "live", from: 0, to: 1, leftValue: 0.20, topValue: 0.5, rightValue: 0.8, bottomValue: 1.0, live: true }
        ]
    }

    function test_dragging(data) {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        dial.wrap = true;
        verify(dial.wrap);
        dial.from = data.from;
        dial.to = data.to;
        dial.live = data.live;

        valueSpy.target = dial;
        verify(valueSpy.valid);

        var moveSpy = createTemporaryObject(signalSpy, testCase, {target: dial, signalName: "moved"});
        verify(moveSpy.valid);

        var minimumExpectedValueCount = data.live ? 2 : 1;

        // drag to the left
        mouseDrag(dial, dial.width / 2, dial.height / 2, -dial.width / 2, 0, Qt.LeftButton);
        fuzzyCompare(dial.value, data.leftValue, 0.1);
        verify(valueSpy.count >= minimumExpectedValueCount);
        valueSpy.clear();
        verify(moveSpy.count > 0);
        moveSpy.clear();

        // drag to the top
        mouseDrag(dial, dial.width / 2, dial.height / 2, 0, -dial.height / 2, Qt.LeftButton);
        fuzzyCompare(dial.value, data.topValue, 0.1);
        verify(valueSpy.count >= minimumExpectedValueCount);
        valueSpy.clear();
        verify(moveSpy.count > 0);
        moveSpy.clear();

        // drag to the right
        mouseDrag(dial, dial.width / 2, dial.height / 2, dial.width / 2, 0, Qt.LeftButton);
        fuzzyCompare(dial.value, data.rightValue, 0.1);
        verify(valueSpy.count >= minimumExpectedValueCount);
        valueSpy.clear();
        verify(moveSpy.count > 0);
        moveSpy.clear();

        // drag to the bottom (* 0.6 to ensure we don't go over to the minimum position)
        mouseDrag(dial, dial.width / 2, dial.height / 2, 10, dial.height / 2, Qt.LeftButton);
        fuzzyCompare(dial.value, data.bottomValue, 0.1);
        verify(valueSpy.count >= minimumExpectedValueCount);
        valueSpy.clear();
        verify(moveSpy.count > 0);
        moveSpy.clear();
    }

    function test_nonWrapping() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        compare(dial.wrap, false);
        dial.value = 0;

        // Ensure that dragging from bottom left to bottom right doesn't work.
        var yPos = dial.height * 0.75;
        mousePress(dial, dial.width * 0.25, yPos, Qt.LeftButton);
        var positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, yPos);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, yPos);
        compare(dial.position, positionAtPress);
        mouseRelease(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);

        // Try the same thing, but a bit higher.
        yPos = dial.height * 0.6;
        mousePress(dial, dial.width * 0.25, yPos, Qt.LeftButton);
        positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, yPos);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, yPos);
        compare(dial.position, positionAtPress);
        mouseRelease(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);

        // Going from below the center of the dial to above it should work (once it gets above the center).
        mousePress(dial, dial.width * 0.25, dial.height * 0.75, Qt.LeftButton);
        positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, dial.height * 0.6);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, dial.height * 0.4);
        verify(dial.position > positionAtPress);
        mouseRelease(dial, dial.width * 0.75, dial.height * 0.3, Qt.LeftButton);
        verify(dial.position > positionAtPress);
    }

    function test_touch() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        var touch = touchEvent(dial);

        // Ensure that dragging from bottom left to bottom right doesn't work.
        var yPos = dial.height * 0.75;
        touch.press(0, dial, dial.width * 0.25, yPos).commit();
        var positionAtPress = dial.position;
        touch.move(0, dial, dial.width * 0.5, yPos).commit();
        compare(dial.position, positionAtPress);
        touch.move(0, dial, dial.width * 0.75, yPos).commit();
        compare(dial.position, positionAtPress);
        touch.release(0, dial, dial.width * 0.75, yPos).commit();
        compare(dial.position, positionAtPress);

        // Try the same thing, but a bit higher.
        yPos = dial.height * 0.6;
        touch.press(0, dial, dial.width * 0.25, yPos).commit();
        positionAtPress = dial.position;
        touch.move(0, dial, dial.width * 0.5, yPos).commit();
        compare(dial.position, positionAtPress);
        touch.move(0, dial, dial.width * 0.75, yPos).commit();
        compare(dial.position, positionAtPress);
        touch.release(0, dial, dial.width * 0.75, yPos).commit();
        compare(dial.position, positionAtPress);

        // Going from below the center of the dial to above it should work (once it gets above the center).
        touch.press(0, dial, dial.width * 0.25, dial.height * 0.75).commit();
        positionAtPress = dial.position;
        touch.move(0, dial, dial.width * 0.5, dial.height * 0.6).commit();
        compare(dial.position, positionAtPress);
        touch.move(0, dial, dial.width * 0.75, dial.height * 0.4).commit();
        verify(dial.position > positionAtPress);
        touch.release(0, dial, dial.width * 0.75, dial.height * 0.3).commit();
        verify(dial.position > positionAtPress);
    }

    function test_multiTouch() {
        var dial1 = createTemporaryObject(dialComponent, testCase);
        verify(dial1);

        var touch = touchEvent(dial1);
        touch.press(0, dial1).commit().move(0, dial1, dial1.width / 4, dial1.height / 4).commit();
        compare(dial1.pressed, true);
        verify(dial1.position > 0.0);

        var pos1Before = dial1.position;

        // second touch point on the same control is ignored
        touch.stationary(0).press(1, dial1, 0, 0).commit()
        touch.stationary(0).move(1, dial1).commit()
        touch.stationary(0).release(1).commit()
        compare(dial1.pressed, true);
        compare(dial1.position, pos1Before);

        var dial2 = createTemporaryObject(dialComponent, testCase, {y: dial1.height});
        verify(dial2);

        // press the second dial
        touch.stationary(0).press(2, dial2, 0, 0).commit();
        compare(dial2.pressed, true);
        compare(dial2.position, 0.0);

        pos1Before = dial1.position;
        var pos2Before = dial2.position;

        // move both dials
        touch.move(0, dial1).move(2, dial2, dial2.width / 4, dial2.height / 4).commit();
        compare(dial1.pressed, true);
        verify(dial1.position !== pos1Before);
        compare(dial2.pressed, true);
        verify(dial2.position !== pos2Before);

        // release both dials
        touch.release(0, dial1).release(2, dial2).commit();
        compare(dial1.pressed, false);
        compare(dial1.value, dial1.position);
        compare(dial2.pressed, false);
        compare(dial2.value, dial2.position);
    }

    property Component focusTest: Component {
        FocusScope {
            signal receivedKeyPress

            Component.onCompleted: forceActiveFocus()
            anchors.fill: parent
            Keys.onPressed: receivedKeyPress()
        }
    }

    SignalSpy {
        id: parentEventSpy
    }

    function test_keyboardNavigation() {
        var dial = createTemporaryObject(dialComponent, testCase);
        verify(dial);

        var focusScope = createTemporaryObject(focusTest, testCase);
        verify(focusScope);

        var moveCount = 0;

        // Tests that we've accepted events that we're interested in.
        parentEventSpy.target = focusScope;
        parentEventSpy.signalName = "receivedKeyPress";

        var moveSpy = createTemporaryObject(signalSpy, testCase, {target: dial, signalName: "moved"});
        verify(moveSpy.valid);

        dial.parent = focusScope;
        compare(dial.activeFocusOnTab, true);
        compare(dial.value, 0);

        dial.focus = true;
        compare(dial.activeFocus, true);
        dial.stepSize = 0.1;

        keyClick(Qt.Key_Left);
        compare(parentEventSpy.count, 0);
        compare(moveSpy.count, moveCount);
        compare(dial.value, 0);

        var oldValue = 0.0;
        var keyPairs = [[Qt.Key_Left, Qt.Key_Right], [Qt.Key_Down, Qt.Key_Up]];
        for (var keyPairIndex = 0; keyPairIndex < 2; ++keyPairIndex) {
            for (var i = 1; i <= 10; ++i) {
                oldValue = dial.value;
                keyClick(keyPairs[keyPairIndex][1]);
                compare(parentEventSpy.count, 0);
                if (oldValue !== dial.value)
                    compare(moveSpy.count, ++moveCount);
                compare(dial.value, dial.stepSize * i);
            }

            compare(dial.value, dial.to);

            for (i = 10; i > 0; --i) {
                oldValue = dial.value;
                keyClick(keyPairs[keyPairIndex][0]);
                compare(parentEventSpy.count, 0);
                if (oldValue !== dial.value)
                    compare(moveSpy.count, ++moveCount);
                compare(dial.value, dial.stepSize * (i - 1));
            }
        }

        dial.value = 0.5;

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(moveSpy.count, ++moveCount);
        compare(dial.value, dial.from);

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(moveSpy.count, moveCount);
        compare(dial.value, dial.from);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(moveSpy.count, ++moveCount);
        compare(dial.value, dial.to);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(moveSpy.count, moveCount);
        compare(dial.value, dial.to);
    }

    function test_snapMode_data(immediate) {
        return [
            { tag: "NoSnap", snapMode: Slider.NoSnap, from: 0, to: 2, values: [0, 0, 1], positions: [0, 0.5, 0.5] },
            { tag: "SnapAlways (0..2)", snapMode: Slider.SnapAlways, from: 0, to: 2, values: [0.0, 0.0, 1.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapAlways (1..3)", snapMode: Slider.SnapAlways, from: 1, to: 3, values: [1.0, 1.0, 2.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapAlways (-1..1)", snapMode: Slider.SnapAlways, from: -1, to: 1, values: [0.0, 0.0, 0.0], positions: [0.5, 0.5, 0.5] },
            { tag: "SnapAlways (1..-1)", snapMode: Slider.SnapAlways, from: 1, to: -1, values: [1.0, 1.0, 0.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (0..2)", snapMode: Slider.SnapOnRelease, from: 0, to: 2, values: [0.0, 0.0, 1.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (1..3)", snapMode: Slider.SnapOnRelease, from: 1, to: 3, values: [1.0, 1.0, 2.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (-1..1)", snapMode: Slider.SnapOnRelease, from: -1, to: 1, values: [0.0, 0.0, 0.0], positions: [immediate ? 0.0 : 0.5, 0.5, 0.5] },
            { tag: "SnapOnRelease (1..-1)", snapMode: Slider.SnapOnRelease, from: 1, to: -1, values: [1.0, 1.0, 0.0], positions: [0.0, 0.5, 0.5] }
        ]
    }

    function test_snapMode_mouse_data() {
        return test_snapMode_data(true)
    }

    function test_snapMode_mouse(data) {
        var dial = createTemporaryObject(dialComponent, testCase, {live: false});
        verify(dial);

        dial.snapMode = data.snapMode;
        dial.from = data.from;
        dial.to = data.to;
        dial.stepSize = 0.2;

        var fuzz = 0.055;

        mousePress(dial, dial.width * 0.25, dial.height * 0.75);
        fuzzyCompare(dial.value, data.values[0], fuzz);
        fuzzyCompare(dial.position, data.positions[0], fuzz);

        mouseMove(dial, dial.width * 0.5, dial.height * 0.25);
        fuzzyCompare(dial.value, data.values[1], fuzz);
        fuzzyCompare(dial.position, data.positions[1], fuzz);

        mouseRelease(dial, dial.width * 0.5, dial.height * 0.25);
        fuzzyCompare(dial.value, data.values[2], fuzz);
        fuzzyCompare(dial.position, data.positions[2], fuzz);
    }

    function test_snapMode_touch_data() {
        return test_snapMode_data(false)
    }

    function test_snapMode_touch(data) {
        var dial = createTemporaryObject(dialComponent, testCase, {live: false});
        verify(dial);

        dial.snapMode = data.snapMode;
        dial.from = data.from;
        dial.to = data.to;
        dial.stepSize = 0.2;

        var fuzz = 0.05;

        var touch = touchEvent(dial);
        touch.press(0, dial, dial.width * 0.25, dial.height * 0.75).commit()
        compare(dial.value, data.values[0]);
        compare(dial.position, data.positions[0]);

        touch.move(0, dial, dial.width * 0.5, dial.height * 0.25).commit();
        fuzzyCompare(dial.value, data.values[1], fuzz);
        fuzzyCompare(dial.position, data.positions[1], fuzz);

        touch.release(0, dial, dial.width * 0.5, dial.height * 0.25).commit();
        fuzzyCompare(dial.value, data.values[2], fuzz);
        fuzzyCompare(dial.position, data.positions[2], fuzz);
    }

    function test_wheel_data() {
        return [
            { tag: "horizontal", orientation: Qt.Horizontal, dx: 120, dy: 0 },
            { tag: "vertical", orientation: Qt.Vertical, dx: 0, dy: 120 }
        ]
    }

    function test_wheel(data) {
        var control = createTemporaryObject(dialComponent, testCase, {wheelEnabled: true, orientation: data.orientation})
        verify(control)

        compare(control.value, 0.0)

        mouseWheel(control, control.width / 2, control.height / 2, data.dx, data.dy)
        compare(control.value, 0.1)
        compare(control.position, 0.1)

        control.stepSize = 0.2

        mouseWheel(control, control.width / 2, control.height / 2, data.dx, data.dy)
        compare(control.value, 0.3)
        compare(control.position, 0.3)

        control.stepSize = 10.0

        mouseWheel(control, control.width / 2, control.height / 2, -data.dx, -data.dy)
        compare(control.value, 0.0)
        compare(control.position, 0.0)

        control.to = 10.0
        control.stepSize = 5.0

        mouseWheel(control, control.width / 2, control.height / 2, data.dx, data.dy)
        compare(control.value, 5.0)
        compare(control.position, 0.5)

        mouseWheel(control, control.width / 2, control.height / 2, 0.5 * data.dx, 0.5 * data.dy)
        compare(control.value, 7.5)
        compare(control.position, 0.75)

        mouseWheel(control, control.width / 2, control.height / 2, -data.dx, -data.dy)
        compare(control.value, 2.5)
        compare(control.position, 0.25)
    }

    function test_nullHandle() {
        var control = createTemporaryObject(dialComponent, testCase)
        verify(control)

        control.handle = null

        mousePress(control)
        verify(control.pressed, true)

        mouseRelease(control)
        compare(control.pressed, false)
    }
}
