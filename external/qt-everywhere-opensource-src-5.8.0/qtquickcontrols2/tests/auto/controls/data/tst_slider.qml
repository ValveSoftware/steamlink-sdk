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
    name: "Slider"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: slider
        Slider { }
    }

    function test_defaults() {
        var control = slider.createObject(testCase)
        verify(control)

        compare(control.stepSize, 0)
        compare(control.snapMode, Slider.NoSnap)
        compare(control.orientation, Qt.Horizontal)

        control.destroy()
    }

    function test_value() {
        var control = slider.createObject(testCase)
        verify(control)

        compare(control.value, 0.0)
        control.value = 0.5
        compare(control.value, 0.5)
        control.value = 1.0
        compare(control.value, 1.0)
        control.value = -1.0
        compare(control.value, 0.0)
        control.value = 2.0
        compare(control.value, 1.0)

        control.destroy()
    }

    function test_range() {
        var control = slider.createObject(testCase, {from: 0, to: 100, value: 50})
        verify(control)

        compare(control.from, 0)
        compare(control.to, 100)
        compare(control.value, 50)
        compare(control.position, 0.5)

        control.value = 1000
        compare(control.value, 100)
        compare(control.position, 1)

        control.value = -1
        compare(control.value, 0)
        compare(control.position, 0)

        control.from = 25
        compare(control.from, 25)
        compare(control.value, 25)
        compare(control.position, 0)

        control.to = 75
        compare(control.to, 75)
        compare(control.value, 25)
        compare(control.position, 0)

        control.value = 50
        compare(control.value, 50)
        compare(control.position, 0.5)

        control.destroy()
    }

    function test_inverted() {
        var control = slider.createObject(testCase, {from: 1.0, to: -1.0})
        verify(control)

        compare(control.from, 1.0)
        compare(control.to, -1.0)
        compare(control.value, 0.0)
        compare(control.position, 0.5)

        control.value = 2.0
        compare(control.value, 1.0)
        compare(control.position, 0.0)

        control.value = -2.0
        compare(control.value, -1.0)
        compare(control.position, 1.0)

        control.value = 0.0
        compare(control.value, 0.0)
        compare(control.position, 0.5)

        control.destroy()
    }

    function test_position() {
        var control = slider.createObject(testCase)
        verify(control)

        compare(control.value, 0.0)
        compare(control.position, 0.0)

        control.value = 0.25
        compare(control.value, 0.25)
        compare(control.position, 0.25)

        control.value = 0.75
        compare(control.value, 0.75)
        compare(control.position, 0.75)

        control.destroy()
    }

    function test_visualPosition() {
        var control = slider.createObject(testCase)
        verify(control)

        compare(control.value, 0.0)
        compare(control.visualPosition, 0.0)

        control.value = 0.25
        compare(control.value, 0.25)
        compare(control.visualPosition, 0.25)

        // RTL locale
        control.locale = Qt.locale("ar_EG")
        compare(control.visualPosition, 0.75)

        // RTL locale + LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.visualPosition, 0.75)

        // LTR locale + LayoutMirroring
        control.locale = Qt.locale("en_US")
        compare(control.visualPosition, 0.75)

        // LTR locale
        control.LayoutMirroring.enabled = false
        compare(control.visualPosition, 0.25)

        // LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.visualPosition, 0.75)

        control.destroy()
    }

    function test_orientation() {
        var control = slider.createObject(testCase)
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
        var control = slider.createObject(testCase, {orientation: data.orientation})
        verify(control)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        mousePress(control, 0, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.0)

        // mininum on the left in horizontal vs. at the bottom in vertical
        mouseMove(control, -control.width, 2 * control.height, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.0)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        verify(control.position, 0.5)

        mouseRelease(control, control.width * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 2)
        compare(control.pressed, false)
        compare(control.value, 0.5)
        compare(control.position, 0.5)

        mousePress(control, control.width, control.height, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.5)
        compare(control.position, 0.5)

        // maximum on the right in horizontal vs. at the top in vertical
        mouseMove(control, control.width * 2, -control.height, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.5)
        compare(control.position, 1.0)

        mouseMove(control, control.width * 0.75, control.height * 0.25, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.5)
        verify(control.position >= 0.75)

        mouseRelease(control, control.width * 0.25, control.height * 0.75, Qt.LeftButton)
        compare(pressedSpy.count, 4)
        compare(control.pressed, false)
        compare(control.value, control.position)
        verify(control.value <= 0.25 && control.value >= 0.0)
        verify(control.position <= 0.25 && control.position >= 0.0)

        // QTBUG-53846
        mouseClick(control, control.width * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 6)
        compare(control.value, 0.5)
        compare(control.position, 0.5)

        control.destroy()
    }

    function test_keys_data() {
        return [
            { tag: "horizontal", orientation: Qt.Horizontal, decrease: Qt.Key_Left, increase: Qt.Key_Right },
            { tag: "vertical", orientation: Qt.Vertical, decrease: Qt.Key_Down, increase: Qt.Key_Up }
        ]
    }

    function test_keys(data) {
        var control = slider.createObject(testCase, {orientation: data.orientation})
        verify(control)

        var pressedCount = 0

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        control.forceActiveFocus()
        verify(control.activeFocus)

        control.value = 0.5

        for (var d1 = 1; d1 <= 10; ++d1) {
            keyPress(data.decrease)
            compare(control.pressed, true)
            compare(pressedSpy.count, ++pressedCount)

            compare(control.value, Math.max(0.0, 0.5 - d1 * 0.1))
            compare(control.value, control.position)

            keyRelease(data.decrease)
            compare(control.pressed, false)
            compare(pressedSpy.count, ++pressedCount)
        }

        for (var i1 = 1; i1 <= 20; ++i1) {
            keyPress(data.increase)
            compare(control.pressed, true)
            compare(pressedSpy.count, ++pressedCount)

            compare(control.value, Math.min(1.0, 0.0 + i1 * 0.1))
            compare(control.value, control.position)

            keyRelease(data.increase)
            compare(control.pressed, false)
            compare(pressedSpy.count, ++pressedCount)
        }

        control.stepSize = 0.25

        for (var d2 = 1; d2 <= 10; ++d2) {
            keyPress(data.decrease)
            compare(control.pressed, true)
            compare(pressedSpy.count, ++pressedCount)

            compare(control.value, Math.max(0.0, 1.0 - d2 * 0.25))
            compare(control.value, control.position)

            keyRelease(data.decrease)
            compare(control.pressed, false)
            compare(pressedSpy.count, ++pressedCount)
        }

        for (var i2 = 1; i2 <= 10; ++i2) {
            keyPress(data.increase)
            compare(control.pressed, true)
            compare(pressedSpy.count, ++pressedCount)

            compare(control.value, Math.min(1.0, 0.0 + i2 * 0.25))
            compare(control.value, control.position)

            keyRelease(data.increase)
            compare(control.pressed, false)
            compare(pressedSpy.count, ++pressedCount)
        }

        control.destroy()
    }

    function test_padding() {
        // test with "unbalanced" paddings (left padding != right padding) to ensure
        // that the slider position calculation is done taking padding into account
        // ==> the position is _not_ 0.5 in the middle of the control
        var control = slider.createObject(testCase, {leftPadding: 10, rightPadding: 20})
        verify(control)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        mousePress(control, 0, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.0)
        compare(control.visualPosition, 0.0)

        mouseMove(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.5)
        compare(control.visualPosition, 0.5)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        verify(control.position > 0.5)
        verify(control.visualPosition > 0.5)

        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 2)
        compare(control.pressed, false)
        compare(control.value, 0.5)
        compare(control.position, 0.5)
        compare(control.visualPosition, 0.5)

        // RTL
        control.value = 0
        control.locale = Qt.locale("ar_EG")

        mousePress(control, 0, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.0)
        compare(control.visualPosition, 1.0)

        mouseMove(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        compare(control.position, 0.5)
        compare(control.visualPosition, 0.5)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.value, 0.0)
        verify(control.position < 0.5)
        verify(control.visualPosition > 0.5)

        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 4)
        compare(control.pressed, false)
        compare(control.value, 0.5)
        compare(control.position, 0.5)
        compare(control.visualPosition, 0.5)

        control.destroy()
    }

    function test_snapMode_data() {
        return [
            { tag: "NoSnap", snapMode: Slider.NoSnap, from: 0, to: 2, values: [0, 0, 0.25], positions: [0, 0.1, 0.1] },
            { tag: "SnapAlways (0..2)", snapMode: Slider.SnapAlways, from: 0, to: 2, values: [0.0, 0.0, 0.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapAlways (1..3)", snapMode: Slider.SnapAlways, from: 1, to: 3, values: [1.0, 1.0, 1.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapAlways (-1..1)", snapMode: Slider.SnapAlways, from: -1, to: 1, values: [0.0, 0.0, -0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapAlways (1..-1)", snapMode: Slider.SnapAlways, from: 1, to: -1, values: [0.0, 0.0,  0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapOnRelease (0..2)", snapMode: Slider.SnapOnRelease, from: 0, to: 2, values: [0.0, 0.0, 0.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapOnRelease (1..3)", snapMode: Slider.SnapOnRelease, from: 1, to: 3, values: [1.0, 1.0, 1.2], positions: [0.0, 0.1, 0.1] },
            { tag: "SnapOnRelease (-1..1)", snapMode: Slider.SnapOnRelease, from: -1, to: 1, values: [0.0, 0.0, -0.8], positions: [0.5, 0.1, 0.1] },
            { tag: "SnapOnRelease (1..-1)", snapMode: Slider.SnapOnRelease, from: 1, to: -1, values: [0.0, 0.0,  0.8], positions: [0.5, 0.1, 0.1] }
        ]
    }

    function test_snapMode(data) {
        var control = slider.createObject(testCase, {snapMode: data.snapMode, from: data.from, to: data.to, stepSize: 0.2})
        verify(control)

        function sliderCompare(left, right) {
            return Math.abs(left - right) < 0.05
        }

        mousePress(control, control.leftPadding)
        compare(control.value, data.values[0])
        compare(control.position, data.positions[0])

        mouseMove(control, control.leftPadding + 0.15 * (control.availableWidth + control.handle.width / 2))

        verify(sliderCompare(control.value, data.values[1]))
        verify(sliderCompare(control.position, data.positions[1]))

        mouseRelease(control, control.leftPadding + 0.15 * (control.availableWidth + control.handle.width / 2))
        verify(sliderCompare(control.value, data.values[2]))
        verify(sliderCompare(control.position, data.positions[2]))

        control.destroy()
    }

    function test_wheel_data() {
        return [
            { tag: "horizontal", orientation: Qt.Horizontal, dx: 120, dy: 0 },
            { tag: "vertical", orientation: Qt.Vertical, dx: 0, dy: 120 }
        ]
    }

    function test_wheel(data) {
        var control = slider.createObject(testCase, {wheelEnabled: true, orientation: data.orientation})
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

        control.destroy()
    }

    function test_valueAt_data() {
        return [
            { tag: "0.0..1.0", from: 0.0, to: 1.0, values: [0.0, 0.2, 0.5, 1.0] },
            { tag: "0..100", from: 0, to: 100, values: [0, 20, 50, 100] },
            { tag: "100..-100", from: 100, to: -100, values: [100, 60, 0, -100] }
        ]
    }

    function test_valueAt(data) {
        var control = slider.createObject(testCase, {from: data.from, to: data.to})
        verify(control)

        compare(control.valueAt(0.0), data.values[0])
        compare(control.valueAt(0.2), data.values[1])
        compare(control.valueAt(0.5), data.values[2])
        compare(control.valueAt(1.0), data.values[3])

        control.destroy()
    }
}
