/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtTest 1.0
import QtQuick 2.1
import "TestUtils.js" as TestUtils

TestCase {
    id: testcase
    name: "Tests_Dial"
    visible: true
    when: windowShown
    width: 400
    height: 400

    function test_instance() {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '');
        verify (dial, "Dial: failed to create an instance")
        verify(dial.__style)
        compare(dial.value, 0.0)
        compare(dial.minimumValue, 0.0)
        compare(dial.maximumValue, 1.0)
        compare(dial.stepSize, 0.0)
        verify(!dial.wrap)
        verify(!dial.activeFocusOnPress)
        verify(!dial.containsMouse)
        verify(!dial.pressed)
        dial.destroy()
    }

    function test_minimumValue() {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '');
        verify (dial, "Dial: failed to create an instance")
        dial.minimumValue = 5
        dial.maximumValue = 10
        dial.value = 2
        compare(dial.minimumValue, 5)
        compare(dial.value, 5)
        dial.destroy()
    }

    function test_maximumValue() {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '');
        verify (dial, "Dial: failed to create an instance")
        dial.minimumValue = 5
        dial.maximumValue = 10
        dial.value = 15
        compare(dial.maximumValue, 10)
        compare(dial.value, 10)
        dial.destroy()
    }

    function test_activeFocusOnPress(){
        var scope = Qt.createQmlObject('import QtQuick 2.2; FocusScope { focus: false }', testcase, '')
        verify(!scope.activeFocus)

        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', scope, '')
        verify (dial, "Dial: failed to create an instance")
        verify(!dial.activeFocusOnPress)
        verify(!dial.activeFocus)
        verify(!scope.activeFocus)
        mousePress(dial, dial.width / 2, dial.height / 2)
        verify(!dial.activeFocus)
        verify(!scope.activeFocus)
        mouseRelease(dial, dial.width / 2, dial.height / 2)
        verify(!dial.activeFocus)
        verify(!scope.activeFocus)

        dial.activeFocusOnPress = true
        verify(dial.activeFocusOnPress)
        verify(!dial.activeFocus)
        verify(!scope.activeFocus)
        mousePress(dial, dial.width / 2, dial.height / 2)
        verify(dial.activeFocus)
        verify(scope.activeFocus)
        mouseRelease(dial, dial.width / 2, dial.height / 2)
        verify(dial.activeFocus)
        verify(scope.activeFocus)

        dial.destroy()
    }

    SignalSpy {
        id: pressSpy
        signalName: "pressedChanged"
    }

    function test_pressed() {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '')
        verify (dial, "Dial: failed to create an instance")

        pressSpy.target = dial
        verify(pressSpy.valid)
        verify(!dial.pressed)

        mousePress(dial, dial.width / 2, dial.height / 2)
        verify(dial.pressed)
        compare(pressSpy.count, 1)

        mouseRelease(dial, dial.width / 2, dial.height / 2)
        verify(!dial.pressed)
        compare(pressSpy.count, 2)

        dial.destroy()
    }

    SignalSpy {
        id: hoverSpy
        signalName: "hoveredChanged"
    }

    function test_hovered() {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '')
        verify (dial, "Dial: failed to create an instance")

        hoverSpy.target = dial
        verify(hoverSpy.valid)
        verify(!dial.hovered)

        mouseMove(dial, dial.width / 2, dial.height / 2)
        verify(dial.hovered)
        compare(hoverSpy.count, 1)

        mouseMove(dial, dial.width * 2, dial.height * 2)
        verify(!dial.hovered)
        compare(hoverSpy.count, 2)

        dial.destroy()
    }

    SignalSpy {
        id: valueSpy
        signalName: "valueChanged"
    }

    function test_dragging_data() {
        return [
            { tag: "default", min: 0, max: 1, leftValue: 0.20, topValue: 0.5, rightValue: 0.8, bottomValue: 1.0 },
            { tag: "scaled2", min: 0, max: 2, leftValue: 0.4, topValue: 1.0, rightValue: 1.6, bottomValue: 2.0 },
            { tag: "scaled1", min: -1, max: 0, leftValue: -0.8, topValue: -0.5, rightValue: -0.2, bottomValue: 0.0 }
        ]
    }

    function test_dragging(data) {
        var dial = Qt.createQmlObject('import QtQuick.Extras 1.4; Dial { }', testcase, '')
        verify(dial, "Dial: failed to create an instance")
        dial.minimumValue = data.min
        dial.maximumValue = data.max

        valueSpy.target = dial
        verify(valueSpy.valid)

        // drag to the left
        mouseDrag(dial, dial.width / 2, dial.height / 2, -dial.width / 2, 0, Qt.LeftButton)
        fuzzyCompare(dial.value, data.leftValue, 0.1)
        verify(valueSpy.count > 0)
        valueSpy.clear()

        // drag to the top
        mouseDrag(dial, dial.width / 2, dial.height / 2, 0, -dial.height / 2, Qt.LeftButton)
        fuzzyCompare(dial.value, data.topValue, 0.1)
        verify(valueSpy.count > 0)
        valueSpy.clear()

        // drag to the right
        mouseDrag(dial, dial.width / 2, dial.height / 2, dial.width / 2, 0, Qt.LeftButton)
        fuzzyCompare(dial.value, data.rightValue, 0.1)
        verify(valueSpy.count > 0)
        valueSpy.clear()

        // drag to the bottom
        mouseDrag(dial, dial.width / 2, dial.height / 2, 0, dial.height / 2, Qt.LeftButton)
        fuzzyCompare(dial.value, data.bottomValue, 0.1)
        verify(valueSpy.count > 0)
        valueSpy.clear()

        dial.destroy()
    }

    function test_outerRadius() {
        var dial = Qt.createQmlObject("import QtQuick.Extras 1.4; Dial { }", testcase, "");
        verify(dial, "Dial: failed to create an instance");
        // Implicit width and height are identical.
        compare(dial.__style.outerRadius, dial.width / 2);

        dial.width = 100;
        dial.height = 250;
        compare(dial.__style.outerRadius, dial.width / 2);

        dial.width = 250;
        dial.height = 100;
        compare(dial.__style.outerRadius, dial.height / 2);

        dial.destroy();
    }

    property Component tickmark: Rectangle {
        objectName: "tickmark" + styleData.index
        implicitWidth: 3
        implicitHeight: 8
        color: "#c8c8c8"
    }

    property Component minorTickmark: Rectangle {
        objectName: "minorTickmark" + styleData.index
        implicitWidth: 2
        implicitHeight: 6
        color: "#c8c8c8"
    }

    property Component tickmarkLabel: Text {
        objectName: "tickmarkLabel" + styleData.index
        text: styleData.value
        color: "#c8c8c8"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    function test_tickmarksVisible() {
        var dial = Qt.createQmlObject("import QtQuick.Extras 1.4; Dial { }", testcase, "");
        verify(dial, "Dial: failed to create an instance");

        dial.__style.minorTickmarkCount = 4;
        dial.__style.tickmark = tickmark;
        dial.__style.minorTickmark = minorTickmark;
        dial.__style.tickmarkLabel = tickmarkLabel;
        verify(TestUtils.findChild(dial, "tickmark0"));
        verify(TestUtils.findChild(dial, "minorTickmark0"));
        verify(TestUtils.findChild(dial, "tickmarkLabel0"));

        dial.tickmarksVisible = false;
        verify(!TestUtils.findChild(dial, "tickmark0"));
        verify(!TestUtils.findChild(dial, "minorTickmark0"));
        verify(!TestUtils.findChild(dial, "tickmarkLabel0"));

        dial.destroy();
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
        var focusScope = focusTest.createObject(testcase);
        verify(focusScope);

        // Tests that we've accepted events that we're interested in.
        parentEventSpy.target = focusScope;
        parentEventSpy.signalName = "receivedKeyPress";

        var dial = Qt.createQmlObject("import QtQuick.Extras 1.4; Dial { }", focusScope, "");
        verify(dial, "Dial: failed to create an instance");
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

            compare(dial.value, dial.maximumValue);

            for (i = 10; i > 0; --i) {
                keyClick(keyPairs[keyPairIndex][0]);
                compare(parentEventSpy.count, 0);
                compare(dial.value, dial.stepSize * (i - 1));
            }
        }

        compare(dial.value, dial.minimumValue);

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.minimumValue);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.maximumValue);

        keyClick(Qt.Key_End);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.maximumValue);

        keyClick(Qt.Key_Home);
        compare(parentEventSpy.count, 0);
        compare(dial.value, dial.minimumValue);

        focusScope.destroy();
    }

    function test_dragToSet() {
        var dial = Qt.createQmlObject("import QtQuick.Extras 1.4; Dial { }", testcase, "");
        verify(dial, "Dial: failed to create an instance");

        dial.__style.__dragToSet = false;

        compare(dial.value, 0);
        var lastValue = dial.value;

        mousePress(dial, dial.width, dial.height / 2, Qt.LeftButton);
        verify(dial.value !== lastValue);
        lastValue = dial.value;
        mouseRelease(dial, dial.width, dial.height / 2, Qt.LeftButton);

        dial.__style.__dragToSet = true;

        mousePress(dial, dial.width / 4, dial.height / 2, Qt.LeftButton);
        verify(dial.value === lastValue);
        mouseRelease(dial, dial.width / 4, dial.height / 2, Qt.LeftButton);

        dial.destroy();
    }

    function test_valueToAngle() {
        var dial = Qt.createQmlObject("import QtQuick.Extras 1.4; Dial { }", testcase, "");
        verify(dial, "Dial: failed to create an instance");

        compare(dial.__style.valueToAngle(0.0), dial.__panel.circularTickmarkLabel.minimumValueAngle);
        compare(dial.__style.valueToAngle(1.0), dial.__panel.circularTickmarkLabel.maximumValueAngle);

        dial.destroy();
    }
}
