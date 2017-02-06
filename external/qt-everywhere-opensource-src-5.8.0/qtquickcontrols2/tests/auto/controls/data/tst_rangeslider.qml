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
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "RangeSlider"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: sliderComponent
        RangeSlider {
            id: slider

            Component.onCompleted: {
                first.handle.objectName = "firstHandle"
                second.handle.objectName = "secondHandle"
            }

            Text {
                text: "1"
                parent: slider.first.handle
                anchors.centerIn: parent
            }

            Text {
                text: "2"
                parent: slider.second.handle
                anchors.centerIn: parent
            }
        }
    }

    function test_defaults() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        compare(control.stepSize, 0)
        compare(control.snapMode, RangeSlider.NoSnap)
        compare(control.orientation, Qt.Horizontal)

        control.destroy()
    }

    function test_values() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        compare(control.first.value, 0.0)
        compare(control.second.value, 1.0)
        control.first.value = 0.5
        compare(control.first.value, 0.5)
        control.first.value = 1.0
        compare(control.first.value, 1.0)
        control.first.value = -1.0
        compare(control.first.value, 0.0)
        control.first.value = 2.0
        compare(control.first.value, 1.0)

        control.first.value = 0
        compare(control.first.value, 0.0)
        control.second.value = 0.5
        compare(control.second.value, 0.5)
        control.first.value = 1
        compare(control.first.value, 0.5)
        control.second.value = 0
        compare(control.second.value, 0.5)

        control.destroy()
    }

    function test_range() {
        var control = sliderComponent.createObject(testCase, { from: 0, to: 100, "first.value": 50, "second.value": 100 })
        verify(control)

        compare(control.from, 0)
        compare(control.to, 100)
        compare(control.first.value, 50)
        compare(control.second.value, 100)
        compare(control.first.position, 0.5)
        compare(control.second.position, 1.0)

        control.first.value = 1000
        compare(control.first.value, 100)
        compare(control.first.position, 1.0)

        control.first.value = -1
        compare(control.first.value, 0)
        compare(control.first.position, 0)

        control.from = 25
        compare(control.from, 25)
        compare(control.first.value, 25)
        compare(control.first.position, 0)

        control.to = 75
        compare(control.to, 75)
        compare(control.second.value, 75)
        compare(control.second.position, 1.0)

        control.first.value = 50
        compare(control.first.value, 50)
        compare(control.first.position, 0.5)

        control.destroy()
    }

    function test_setValues() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        compare(control.from, 0)
        compare(control.to, 1)
        compare(control.first.value, 0)
        compare(control.second.value, 1)
        compare(control.first.position, 0.0)
        compare(control.second.position, 1.0)

        control.setValues(100, 200)
        compare(control.first.value, 1)
        compare(control.second.value, 1)
        compare(control.first.position, 1.0)
        compare(control.second.position, 1.0)

        control.to = 300;
        control.setValues(100, 200)
        compare(control.first.value, 100)
        compare(control.second.value, 200)
        compare(control.first.position, 0.333333)
        compare(control.second.position, 0.666666)

        control.destroy()
    }

    function test_inverted() {
        var control = sliderComponent.createObject(testCase, { from: 1.0, to: -1.0 })
        verify(control)

        compare(control.from, 1.0)
        compare(control.to, -1.0)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.second.value, 0.0);
        compare(control.second.position, 0.5);

        control.first.value = 2.0
        compare(control.first.value, 1.0)
        compare(control.first.position, 0.0)
        compare(control.second.value, 0.0);
        compare(control.second.position, 0.5);

        control.first.value = -2.0
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.second.value, 0.0);
        compare(control.second.position, 0.5);

        control.first.value = 0.0
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.second.value, 0.0);
        compare(control.second.position, 0.5);

        control.destroy()
    }

    function test_visualPosition() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.first.visualPosition, 0.0)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)
        compare(control.second.visualPosition, 1.0)

        control.first.value = 0.25
        compare(control.first.value, 0.25)
        compare(control.first.position, 0.25)
        compare(control.first.visualPosition, 0.25)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)
        compare(control.second.visualPosition, 1.0)

        // RTL locale
        control.locale = Qt.locale("ar_EG")
        compare(control.first.visualPosition, 0.75)
        compare(control.second.visualPosition, 0.0)

        // RTL locale + LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.first.visualPosition, 0.75)
        compare(control.second.visualPosition, 0.0)

        // LTR locale + LayoutMirroring
        control.locale = Qt.locale("en_US")
        compare(control.first.visualPosition, 0.75)
        compare(control.second.visualPosition, 0.0)

        // LTR locale
        control.LayoutMirroring.enabled = false
        compare(control.first.visualPosition, 0.25)
        compare(control.second.visualPosition, 1.0)

        // LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.first.visualPosition, 0.75)
        compare(control.second.visualPosition, 0.0)

        control.destroy()
    }

    function test_orientation() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        compare(control.orientation, Qt.Horizontal)
        verify(control.width > control.height)
        control.orientation = Qt.Vertical
        compare(control.orientation, Qt.Vertical)
        verify(control.width < control.height)

        control.destroy()
    }

    function test_mouse_data() {
        return [
            { tag: "horizontal", orientation: Qt.Horizontal },
            { tag: "vertical", orientation: Qt.Vertical }
        ]
    }

    function test_mouse(data) {
        var control = sliderComponent.createObject(testCase, { orientation: data.orientation })
        verify(control)

        var firstPressedSpy = signalSpy.createObject(control, {target: control.first, signalName: "pressedChanged"})
        verify(firstPressedSpy.valid)

        var secondPressedSpy = signalSpy.createObject(control, {target: control.second, signalName: "pressedChanged"})
        verify(secondPressedSpy.valid)

        mousePress(control, control.width * 0.25, control.height * 0.75, Qt.LeftButton)
        compare(firstPressedSpy.count, 1)
        compare(secondPressedSpy.count, 0)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mouseRelease(control, control.width * 0.25, control.height * 0.75, Qt.LeftButton)
        compare(firstPressedSpy.count, 2)
        compare(secondPressedSpy.count, 0)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mousePress(control, control.width * 0.75, control.height * 0.25, Qt.LeftButton)
        compare(firstPressedSpy.count, 2)
        compare(secondPressedSpy.count, 1)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, true)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mouseRelease(control, control.width * 0.75, control.height * 0.25, Qt.LeftButton)
        compare(firstPressedSpy.count, 2)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mousePress(control, 0, control.height, Qt.LeftButton)
        compare(firstPressedSpy.count, 3)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mouseRelease(control, 0, control.height, Qt.LeftButton)
        compare(firstPressedSpy.count, 4)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        compare(firstPressedSpy.count, 5)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)

        var horizontal = control.orientation === Qt.Horizontal
        var toX = horizontal ? control.width * 0.5 : control.first.handle.x
        var toY = horizontal ? control.first.handle.y : control.height * 0.5
        mouseMove(control, toX, toY, Qt.LeftButton)
        compare(firstPressedSpy.count, 5)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)
        compare(control.second.visualPosition, horizontal ? 1.0 : 0.0)

        mouseRelease(control, toX, toY, Qt.LeftButton)
        compare(firstPressedSpy.count, 6)
        compare(secondPressedSpy.count, 2)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.5)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)
        compare(control.second.pressed, false)
        compare(control.second.value, 1.0)
        compare(control.second.position, 1.0)
        compare(control.second.visualPosition, horizontal ? 1.0 : 0.0)

        control.destroy()
    }

    function test_overlappingHandles() {
        var control = sliderComponent.createObject(testCase, { orientation: data.orientation })
        verify(control)

        // By default, we force the second handle to be after the first in
        // terms of stacking order *and* z value.
        compare(control.second.handle.z, 1)
        compare(control.first.handle.z, 0)
        control.first.value = 0
        control.second.value = 0

        // Both are at the same position, so it doesn't matter whose coordinates we use.
        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        verify(control.second.pressed)
        compare(control.second.handle.z, 1)
        compare(control.first.handle.z, 0)

        // Move the second handle out of the way.
        mouseMove(control, control.width, control.first.handle.y, Qt.LeftButton)
        mouseRelease(control, control.width, control.first.handle.y, Qt.LeftButton)
        verify(!control.second.pressed)
        compare(control.second.value, 1.0)
        compare(control.second.handle.z, 1)
        compare(control.first.handle.z, 0)

        // Move the first handle on top of the second.
        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        verify(control.first.pressed)
        compare(control.first.handle.z, 1)
        compare(control.second.handle.z, 0)

        mouseMove(control, control.width, control.first.handle.y, Qt.LeftButton)
        mouseRelease(control, control.width, control.first.handle.y, Qt.LeftButton)
        verify(!control.first.pressed)
        compare(control.first.handle.z, 1)
        compare(control.second.handle.z, 0)

        // The most recently pressed handle (the first) should have the higher z value.
        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        verify(control.first.pressed)
        compare(control.first.handle.z, 1)
        compare(control.second.handle.z, 0)

        mouseRelease(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        verify(!control.first.pressed)
        compare(control.first.handle.z, 1)
        compare(control.second.handle.z, 0)

        control.destroy()
    }

    function test_keys_data() {
        return [
            { tag: "horizontal", orientation: Qt.Horizontal, decrease: Qt.Key_Left, increase: Qt.Key_Right },
            { tag: "vertical", orientation: Qt.Vertical, decrease: Qt.Key_Down, increase: Qt.Key_Up }
        ]
    }

    function test_keys(data) {
        var control = sliderComponent.createObject(testCase, { orientation: data.orientation })
        verify(control)

        var pressedCount = 0

        var firstPressedSpy = signalSpy.createObject(control, {target: control.first, signalName: "pressedChanged"})
        verify(firstPressedSpy.valid)

        control.first.handle.forceActiveFocus()
        verify(control.first.handle.activeFocus)

        control.first.value = 0.5

        for (var d1 = 1; d1 <= 10; ++d1) {
            keyPress(data.decrease)
            compare(control.first.pressed, true)
            compare(firstPressedSpy.count, ++pressedCount)

            compare(control.first.value, Math.max(0.0, 0.5 - d1 * 0.1))
            compare(control.first.value, control.first.position)

            keyRelease(data.decrease)
            compare(control.first.pressed, false)
            compare(firstPressedSpy.count, ++pressedCount)
        }

        for (var i1 = 1; i1 <= 20; ++i1) {
            keyPress(data.increase)
            compare(control.first.pressed, true)
            compare(firstPressedSpy.count, ++pressedCount)

            compare(control.first.value, Math.min(1.0, 0.0 + i1 * 0.1))
            compare(control.first.value, control.first.position)

            keyRelease(data.increase)
            compare(control.first.pressed, false)
            compare(firstPressedSpy.count, ++pressedCount)
        }

        control.first.value = 0;
        control.stepSize = 0.25

        pressedCount = 0;
        var secondPressedSpy = signalSpy.createObject(control, {target: control.second, signalName: "pressedChanged"})
        verify(secondPressedSpy.valid)

        control.second.handle.forceActiveFocus()
        verify(control.second.handle.activeFocus)

        for (var d2 = 1; d2 <= 10; ++d2) {
            keyPress(data.decrease)
            compare(control.second.pressed, true)
            compare(secondPressedSpy.count, ++pressedCount)

            compare(control.second.value, Math.max(0.0, 1.0 - d2 * 0.25))
            compare(control.second.value, control.second.position)

            keyRelease(data.decrease)
            compare(control.second.pressed, false)
            compare(secondPressedSpy.count, ++pressedCount)
        }

        for (var i2 = 1; i2 <= 10; ++i2) {
            keyPress(data.increase)
            compare(control.second.pressed, true)
            compare(secondPressedSpy.count, ++pressedCount)

            compare(control.second.value, Math.min(1.0, 0.0 + i2 * 0.25))
            compare(control.second.value, control.second.position)

            keyRelease(data.increase)
            compare(control.second.pressed, false)
            compare(secondPressedSpy.count, ++pressedCount)
        }

        control.destroy()
    }

    function test_padding() {
        // test with "unbalanced" paddings (left padding != right padding) to ensure
        // that the slider position calculation is done taking padding into account
        // ==> the position is _not_ 0.5 in the middle of the control
        var control = sliderComponent.createObject(testCase, { leftPadding: 10, rightPadding: 20 })
        verify(control)

        var firstPressedSpy = signalSpy.createObject(control, {target: control.first, signalName: "pressedChanged"})
        verify(firstPressedSpy.valid)

        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        compare(firstPressedSpy.count, 1)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.first.visualPosition, 0.0)

        mouseMove(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(firstPressedSpy.count, 1)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(firstPressedSpy.count, 1)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        verify(control.first.position > 0.5)
        verify(control.first.visualPosition > 0.5)

        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(firstPressedSpy.count, 2)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.5)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)

        // RTL
        control.first.value = 0
        control.locale = Qt.locale("ar_EG")

        mousePress(control, control.first.handle.x, control.first.handle.y, Qt.LeftButton)
        compare(firstPressedSpy.count, 3)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.0)
        compare(control.first.visualPosition, 1.0)

        mouseMove(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(firstPressedSpy.count, 3)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(firstPressedSpy.count, 3)
        compare(control.first.pressed, true)
        compare(control.first.value, 0.0)
        verify(control.first.position < 0.5)
        verify(control.first.visualPosition > 0.5)

        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(firstPressedSpy.count, 4)
        compare(control.first.pressed, false)
        compare(control.first.value, 0.5)
        compare(control.first.position, 0.5)
        compare(control.first.visualPosition, 0.5)

        control.destroy()
    }

    function test_snapMode_data() {
        return [
            { tag: "NoSnap", snapMode: RangeSlider.NoSnap, from: 0, to: 2, values: [0, 0, 0.25], positions: [0, 0.1, 0.1] },
            { tag: "SnapAlways (0..2)", snapMode: RangeSlider.SnapAlways, from: 0, to: 2, values: [0.0, 0.0, 0.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapAlways (1..3)", snapMode: RangeSlider.SnapAlways, from: 1, to: 3, values: [1.0, 1.0, 1.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapAlways (-1..1)", snapMode: RangeSlider.SnapAlways, from: -1, to: 1, values: [0.0, 0.0, -0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapAlways (1..-1)", snapMode: RangeSlider.SnapAlways, from: 1, to: -1, values: [0.0, 0.0,  0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapOnRelease (0..2)", snapMode: RangeSlider.SnapOnRelease, from: 0, to: 2, values: [0.0, 0.0, 0.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapOnRelease (1..3)", snapMode: RangeSlider.SnapOnRelease, from: 1, to: 3, values: [1.0, 1.0, 1.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapOnRelease (-1..1)", snapMode: RangeSlider.SnapOnRelease, from: -1, to: 1, values: [0.0, 0.0, -0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapOnRelease (1..-1)", snapMode: RangeSlider.SnapOnRelease, from: 1, to: -1, values: [0.0, 0.0,  0.8], positions: [0.5, 0.1, 0.1] }
        ]
    }

    function test_snapMode(data) {
        var control = sliderComponent.createObject(testCase, {snapMode: data.snapMode, from: data.from, to: data.to, stepSize: 0.2})
        verify(control)

        control.first.value = 0
        control.second.value = data.to

        function sliderCompare(left, right) {
            return Math.abs(left - right) < 0.05
        }

        mousePress(control, control.first.handle.x, control.first.handle.y)
        compare(control.first.pressed, true)
        compare(control.first.value, data.values[0])
        compare(control.first.position, data.positions[0])

        mouseMove(control, control.leftPadding + 0.15 * (control.availableWidth + control.first.handle.width / 2))
        compare(control.first.pressed, true)
        verify(sliderCompare(control.first.value, data.values[1]))
        verify(sliderCompare(control.first.position, data.positions[1]))

        mouseRelease(control, control.leftPadding + 0.15 * (control.availableWidth + control.first.handle.width / 2))
        compare(control.first.pressed, false)
        verify(sliderCompare(control.first.value, data.values[2]))
        verify(sliderCompare(control.first.position, data.positions[2]))

        control.destroy()
    }

    function test_focus() {
        var control = sliderComponent.createObject(testCase)
        verify(control)

        waitForRendering(control)
        compare(control.activeFocus, false)

        // focus is forwarded to the first handle
        control.forceActiveFocus()
        compare(control.activeFocus, true)
        compare(control.first.handle.activeFocus, true)
        compare(control.second.handle.activeFocus, false)

        // move focus to the second handle
        control.second.handle.forceActiveFocus()
        compare(control.activeFocus, true)
        compare(control.first.handle.activeFocus, false)
        compare(control.second.handle.activeFocus, true)

        // clear focus
        control.focus = false
        compare(control.activeFocus, false)
        compare(control.first.handle.activeFocus, false)
        compare(control.second.handle.activeFocus, false)

        // focus is forwarded to the second handle (where it previously was in the focus scope)
        control.forceActiveFocus()
        compare(control.activeFocus, true)
        compare(control.first.handle.activeFocus, false)
        compare(control.second.handle.activeFocus, true)

        control.destroy()
    }

    function test_hover_data() {
        return [
            { tag: "first:true", node: "first", hoverEnabled: true },
            { tag: "first:false", node: "first", hoverEnabled: false },
            { tag: "second:true", node: "second", hoverEnabled: true },
            { tag: "second:false", node: "second", hoverEnabled: false }
        ]
    }

    function test_hover(data) {
        var control = sliderComponent.createObject(testCase, {hoverEnabled: data.hoverEnabled})
        verify(control)

        var node = control[data.node]
        compare(control.hovered, false)
        compare(node.hovered, false)

        mouseMove(control, node.handle.x + node.handle.width / 2, node.handle.y + node.handle.height / 2)
        compare(control.hovered, data.hoverEnabled)
        compare(node.hovered, data.hoverEnabled && node.handle.enabled)

        mouseMove(control, node.handle.x - 1, node.handle.y - 1)
        compare(node.hovered, false)

        control.destroy()
    }
}
