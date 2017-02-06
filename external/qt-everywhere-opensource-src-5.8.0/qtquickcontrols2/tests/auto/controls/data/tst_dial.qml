/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
import QtQuick.Controls 2.1

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

    property var dial: null

    function init() {
        dial = dialComponent.createObject(testCase);
        verify(dial, "Dial: failed to create an instance");
    }

    function cleanup() {
        if (dial)
            dial.destroy();
    }

    function test_instance() {
        compare(dial.value, 0.0);
        compare(dial.from, 0.0);
        compare(dial.to, 1.0);
        compare(dial.stepSize, 0.0);
        verify(dial.activeFocusOnTab);
        verify(!dial.pressed);
    }

    function test_value() {
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
        dial.destroy();
        dial = dialComponent.createObject(testCase, { from: 1.0, to: -1.0 });
        verify(dial, "Dial: failed to create an instance");
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
        pressSpy.target = dial;
        verify(pressSpy.valid);
        verify(!dial.pressed);

        mousePress(dial, dial.width / 2, dial.height / 2);
        verify(dial.pressed);
        compare(pressSpy.count, 1);

        mouseRelease(dial, dial.width / 2, dial.height / 2);
        verify(!dial.pressed);
        compare(pressSpy.count, 2);
    }

    SignalSpy {
        id: valueSpy
        signalName: "valueChanged"
    }

    function test_dragging_data() {
        return [
            { tag: "default", from: 0, to: 1, leftValue: 0.20, topValue: 0.5, rightValue: 0.8, bottomValue: 1.0 },
            { tag: "scaled2", from: 0, to: 2, leftValue: 0.4, topValue: 1.0, rightValue: 1.6, bottomValue: 2.0 },
            { tag: "scaled1", from: -1, to: 0, leftValue: -0.8, topValue: -0.5, rightValue: -0.2, bottomValue: 0.0 }
        ]
    }

    function test_dragging(data) {
        dial.wrap = true;
        verify(dial.wrap);
        dial.from = data.from;
        dial.to = data.to;

        valueSpy.target = dial;
        verify(valueSpy.valid);

        // drag to the left
        mouseDrag(dial, dial.width / 2, dial.height / 2, -dial.width / 2, 0, Qt.LeftButton);
        fuzzyCompare(dial.value, data.leftValue, 0.1);
        verify(valueSpy.count > 0);
        valueSpy.clear();

        // drag to the top
        mouseDrag(dial, dial.width / 2, dial.height / 2, 0, -dial.height / 2, Qt.LeftButton);
        fuzzyCompare(dial.value, data.topValue, 0.1);
        verify(valueSpy.count > 0);
        valueSpy.clear();

        // drag to the right
        mouseDrag(dial, dial.width / 2, dial.height / 2, dial.width / 2, 0, Qt.LeftButton);
        fuzzyCompare(dial.value, data.rightValue, 0.1);
        verify(valueSpy.count > 0);
        valueSpy.clear();

        // drag to the bottom (* 0.6 to ensure we don't go over to the minimum position)
        mouseDrag(dial, dial.width / 2, dial.height / 2, 10, dial.height / 2, Qt.LeftButton);
        fuzzyCompare(dial.value, data.bottomValue, 0.1);
        verify(valueSpy.count > 0);
        valueSpy.clear();
    }

    function test_nonWrapping() {
        compare(dial.wrap, false);
        dial.value = 0;

        // Ensure that dragging from bottom left to bottom right doesn't work.
        var yPos = dial.height * 0.75;
        mousePress(dial, dial.width * 0.25, yPos, Qt.LeftButton);
        var positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);
        mouseRelease(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);

        // Try the same thing, but a bit higher.
        yPos = dial.height * 0.6;
        mousePress(dial, dial.width * 0.25, yPos, Qt.LeftButton);
        positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);
        mouseRelease(dial, dial.width * 0.75, yPos, Qt.LeftButton);
        compare(dial.position, positionAtPress);

        // Going from below the center of the dial to above it should work (once it gets above the center).
        mousePress(dial, dial.width * 0.25, dial.height * 0.75, Qt.LeftButton);
        positionAtPress = dial.position;
        mouseMove(dial, dial.width * 0.5, dial.height * 0.6, Qt.LeftButton);
        compare(dial.position, positionAtPress);
        mouseMove(dial, dial.width * 0.75, dial.height * 0.4, Qt.LeftButton);
        verify(dial.position > positionAtPress);
        mouseRelease(dial, dial.width * 0.75, dial.height * 0.3, Qt.LeftButton);
        verify(dial.position > positionAtPress);
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
        var focusScope = focusTest.createObject(testCase);
        verify(focusScope);

        // Tests that we've accepted events that we're interested in.
        parentEventSpy.target = focusScope;
        parentEventSpy.signalName = "receivedKeyPress";

        dial.parent = focusScope;
        compare(dial.activeFocusOnTab, true);
        compare(dial.value, 0);

        dial.focus = true;
        compare(dial.activeFocus, true);
        dial.stepSize = 0.1;

        keyClick(Qt.Key_Left);
        compare(parentEventSpy.count, 0);
        compare(dial.value, 0);


        var keyPairs = [[Qt.Key_Left, Qt.Key_Right], [Qt.Key_Down, Qt.Key_Up]];
        for (var keyPairIndex = 0; keyPairIndex < 2; ++keyPairIndex) {
            for (var i = 1; i <= 10; ++i) {
                keyClick(keyPairs[keyPairIndex][1]);
                compare(parentEventSpy.count, 0);
                compare(dial.value, dial.stepSize * i);
            }

            compare(dial.value, dial.to);

            for (i = 10; i > 0; --i) {
                keyClick(keyPairs[keyPairIndex][0]);
                compare(parentEventSpy.count, 0);
                compare(dial.value, dial.stepSize * (i - 1));
            }
        }

        compare(dial.value, dial.from);

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.from);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.to);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.to);

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.from);

        focusScope.destroy();
    }

    function test_snapMode_data() {
        return [
            { tag: "NoSnap", snapMode: Slider.NoSnap, from: 0, to: 2, values: [0, 0, 1], positions: [0, 0.5, 0.5] },
            { tag: "SnapAlways (0..2)", snapMode: Slider.SnapAlways, from: 0, to: 2, values: [0.0, 0.0, 1.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapAlways (1..3)", snapMode: Slider.SnapAlways, from: 1, to: 3, values: [1.0, 1.0, 2.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapAlways (-1..1)", snapMode: Slider.SnapAlways, from: -1, to: 1, values: [0.0, 0.0, 0.0], positions: [0.5, 0.5, 0.5] },
            { tag: "SnapAlways (1..-1)", snapMode: Slider.SnapAlways, from: 1, to: -1, values: [1.0, 1.0, 0.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (0..2)", snapMode: Slider.SnapOnRelease, from: 0, to: 2, values: [0.0, 0.0, 1.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (1..3)", snapMode: Slider.SnapOnRelease, from: 1, to: 3, values: [1.0, 1.0, 2.0], positions: [0.0, 0.5, 0.5] },
            { tag: "SnapOnRelease (-1..1)", snapMode: Slider.SnapOnRelease, from: -1, to: 1, values: [0.0, 0.0, 0.0], positions: [0.5, 0.5, 0.5] },
            { tag: "SnapOnRelease (1..-1)", snapMode: Slider.SnapOnRelease, from: 1, to: -1, values: [1.0, 1.0, 0.0], positions: [0.0, 0.5, 0.5] }
        ]
    }

    function test_snapMode(data) {
        dial.snapMode = data.snapMode;
        dial.from = data.from;
        dial.to = data.to;
        dial.stepSize = 0.2;

        var fuzz = 0.05;

        mousePress(dial, dial.width * 0.25, dial.height * 0.75);
        compare(dial.value, data.values[0]);
        compare(dial.position, data.positions[0]);

        mouseMove(dial, dial.width * 0.5, dial.height * 0.25);
        fuzzyCompare(dial.value, data.values[1], fuzz);
        fuzzyCompare(dial.position, data.positions[1], fuzz);

        mouseRelease(dial, dial.width * 0.5, dial.height * 0.25);
        fuzzyCompare(dial.value, data.values[2], fuzz);
        fuzzyCompare(dial.position, data.positions[2], fuzz);
    }
}
