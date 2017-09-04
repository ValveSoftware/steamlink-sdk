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
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "ScrollBar"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: scrollBar
        ScrollBar { padding: 0 }
    }

    Component {
        id: flickable
        Flickable {
            width: 100
            height: 100
            contentWidth: 200
            contentHeight: 200
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalAndVerticalFlick
        }
    }

    function test_attach() {
        var container = createTemporaryObject(flickable, testCase)
        verify(container)
        waitForRendering(container)

        var vertical = scrollBar.createObject()
        verify(!vertical.parent)
        compare(vertical.size, 0.0)
        compare(vertical.position, 0.0)
        compare(vertical.active, false)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.x, 0)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        verify(vertical.height > 0)

        container.ScrollBar.vertical = vertical
        compare(vertical.parent, container)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(vertical.x, container.width - vertical.width)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        compare(vertical.height, container.height)
        // vertical scroll bar follows flickable's width
        container.width += 10
        compare(vertical.x, container.width - vertical.width)
        vertical.implicitWidth -= 2
        compare(vertical.x, container.width - vertical.width)
        // ...unless explicitly positioned
        vertical.x = 123
        container.width += 10
        compare(vertical.x, 123)

        var horizontal = createTemporaryObject(scrollBar, null)
        verify(!horizontal.parent)
        compare(horizontal.size, 0.0)
        compare(horizontal.position, 0.0)
        compare(horizontal.active, false)
        compare(horizontal.orientation, Qt.Vertical)
        compare(horizontal.x, 0)
        compare(horizontal.y, 0)
        verify(horizontal.width > 0)
        verify(horizontal.height > 0)

        container.ScrollBar.horizontal = horizontal
        compare(horizontal.parent, container)
        compare(horizontal.orientation, Qt.Horizontal)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)
        compare(horizontal.x, 0)
        compare(horizontal.y, container.height - horizontal.height)
        compare(horizontal.width, container.width)
        verify(horizontal.height > 0)
        // horizontal scroll bar follows flickable's height
        container.height += 10
        compare(horizontal.y, container.height - horizontal.height)
        horizontal.implicitHeight -= 2
        compare(horizontal.y, container.height - horizontal.height)
        // ...unless explicitly positioned
        horizontal.y = 123
        container.height += 10
        compare(horizontal.y, 123)

        var velocity = container.maximumFlickVelocity

        compare(container.flicking, false)
        container.flick(-velocity, -velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        compare(container.flicking, false)
        container.flick(velocity, velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        var oldY = vertical.y
        var oldHeight = vertical.height
        vertical.parent = testCase
        vertical.y -= 10
        container.height += 10
        compare(vertical.y, oldY - 10)
        compare(vertical.height, oldHeight)

        var oldX = horizontal.x
        var oldWidth = horizontal.width
        horizontal.parent = testCase
        horizontal.x -= 10
        container.width += 10
        compare(horizontal.x, oldX - 10)
        compare(horizontal.width, oldWidth)
    }

    function test_mouse_data() {
        return [
            { tag: "horizontal", properties: { visible: true, orientation: Qt.Horizontal, width: testCase.width } },
            { tag: "vertical", properties: { visible: true, orientation: Qt.Vertical, height: testCase.height } }
        ]
    }

    function test_mouse(data) {
        var control = createTemporaryObject(scrollBar, testCase, data.properties)
        verify(control)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        mousePress(control, 0, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        mouseMove(control, -control.width, -control.height, 0)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        verify(control.position, 0.5)

        mouseRelease(control, control.width * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 2)
        compare(control.pressed, false)
        compare(control.position, 0.5)

        mousePress(control, control.width, control.height, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 1.0)

        mouseMove(control, control.width * 2, control.height * 2, 0)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 1.0)

        mouseMove(control, control.width * 0.75, control.height * 0.75, 0)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 0.75)

        mouseRelease(control, control.width * 0.25, control.height * 0.25, Qt.LeftButton)
        compare(pressedSpy.count, 4)
        compare(control.pressed, false)
        compare(control.position, 0.25)
    }

    function test_touch_data() {
        return [
            { tag: "horizontal", properties: { visible: true, orientation: Qt.Horizontal, width: testCase.width } },
            { tag: "vertical", properties: { visible: true, orientation: Qt.Vertical, height: testCase.height } }
        ]
    }

    function test_touch(data) {
        var control = createTemporaryObject(scrollBar, testCase, data.properties)
        verify(control)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        var touch = touchEvent(control)

        touch.press(0, control, 0, 0).commit()
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        touch.move(0, control, -control.width, -control.height).commit()
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        touch.move(0, control, control.width * 0.5, control.height * 0.5).commit()
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        verify(control.position, 0.5)

        touch.release(0, control, control.width * 0.5, control.height * 0.5).commit()
        compare(pressedSpy.count, 2)
        compare(control.pressed, false)
        compare(control.position, 0.5)

        touch.press(0, control, control.width, control.height).commit()
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 0.5)

        touch.move(0, control, control.width * 2, control.height * 2).commit()
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 1.0)

        touch.move(0, control, control.width * 0.75, control.height * 0.75).commit()
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 0.75)

        touch.release(0, control, control.width * 0.25, control.height * 0.25).commit()
        compare(pressedSpy.count, 4)
        compare(control.pressed, false)
        compare(control.position, 0.25)
    }

    function test_multiTouch() {
        var control1 = createTemporaryObject(scrollBar, testCase)
        verify(control1)

        var pressedCount1 = 0
        var movedCount1 = 0

        var pressedSpy1 = signalSpy.createObject(control1, {target: control1, signalName: "pressedChanged"})
        verify(pressedSpy1.valid)

        var positionSpy1 = signalSpy.createObject(control1, {target: control1, signalName: "positionChanged"})
        verify(positionSpy1.valid)

        var touch = touchEvent(control1)
        touch.press(0, control1, 0, 0).commit().move(0, control1, control1.width, control1.height).commit()

        compare(pressedSpy1.count, ++pressedCount1)
        compare(positionSpy1.count, ++movedCount1)
        compare(control1.pressed, true)
        compare(control1.position, 1.0)

        // second touch point on the same control is ignored
        touch.stationary(0).press(1, control1, 0, 0).commit()
        touch.stationary(0).move(1, control1).commit()
        touch.stationary(0).release(1).commit()

        compare(pressedSpy1.count, pressedCount1)
        compare(positionSpy1.count, movedCount1)
        compare(control1.pressed, true)
        compare(control1.position, 1.0)

        var control2 = createTemporaryObject(scrollBar, testCase, {y: control1.height})
        verify(control2)

        var pressedCount2 = 0
        var movedCount2 = 0

        var pressedSpy2 = signalSpy.createObject(control2, {target: control2, signalName: "pressedChanged"})
        verify(pressedSpy2.valid)

        var positionSpy2 = signalSpy.createObject(control2, {target: control2, signalName: "positionChanged"})
        verify(positionSpy2.valid)

        // press the second scrollbar
        touch.stationary(0).press(2, control2, 0, 0).commit()

        compare(pressedSpy2.count, ++pressedCount2)
        compare(positionSpy2.count, movedCount2)
        compare(control2.pressed, true)
        compare(control2.position, 0.0)

        compare(pressedSpy1.count, pressedCount1)
        compare(positionSpy1.count, movedCount1)
        compare(control1.pressed, true)
        compare(control1.position, 1.0)

        // move both scrollbars
        touch.move(0, control1).move(2, control2).commit()

        compare(pressedSpy2.count, pressedCount2)
        compare(positionSpy2.count, ++movedCount2)
        compare(control2.pressed, true)
        compare(control2.position, 0.5)

        compare(pressedSpy1.count, pressedCount1)
        compare(positionSpy1.count, ++movedCount1)
        compare(control1.pressed, true)
        compare(control1.position, 0.5)

        // release both scrollbars
        touch.release(0, control1).release(2, control2).commit()

        compare(pressedSpy2.count, ++pressedCount2)
        compare(positionSpy2.count, movedCount2)
        compare(control2.pressed, false)
        compare(control2.position, 0.5)

        compare(pressedSpy1.count, ++pressedCount1)
        compare(positionSpy1.count, movedCount1)
        compare(control1.pressed, false)
        compare(control1.position, 0.5)
    }

    function test_increase_decrease_data() {
        return [
            { tag: "increase:active", increase: true, active: true },
            { tag: "decrease:active", increase: false, active: true },
            { tag: "increase:inactive", increase: true, active: false },
            { tag: "decrease:inactive", increase: false, active: false }
        ]
    }

    function test_increase_decrease(data) {
        var control = createTemporaryObject(scrollBar, testCase, {position: 0.5, active: data.active})
        verify(control)

        if (data.increase) {
            control.increase()
            compare(control.position, 0.6)
        } else {
            control.decrease()
            compare(control.position, 0.4)
        }
        compare(control.active, data.active)
    }

    function test_stepSize_data() {
        return [
            { tag: "0.0", stepSize: 0.0 },
            { tag: "0.1", stepSize: 0.1 },
            { tag: "0.5", stepSize: 0.5 }
        ]
    }

    function test_stepSize(data) {
        var control = createTemporaryObject(scrollBar, testCase, {stepSize: data.stepSize})
        verify(control)

        compare(control.stepSize, data.stepSize)
        compare(control.position, 0.0)

        var count = 10
        if (data.stepSize !== 0.0)
            count = 1.0 / data.stepSize

        // increase until 1.0
        for (var i = 1; i <= count; ++i) {
            control.increase()
            compare(control.position, i / count)
        }
        control.increase()
        compare(control.position, 1.0)

        // decrease until 0.0
        for (var d = count - 1; d >= 0; --d) {
            control.decrease()
            compare(control.position, d / count)
        }
        control.decrease()
        compare(control.position, 0.0)
    }

    function test_padding_data() {
        return [
            { tag: "horizontal", properties: { visible: true, orientation: Qt.Horizontal, width: testCase.width, leftPadding: testCase.width * 0.1 } },
            { tag: "vertical", properties: { visible: true, orientation: Qt.Vertical, height: testCase.height, topPadding: testCase.height * 0.1 } }
        ]
    }

    function test_padding(data) {
        var control = createTemporaryObject(scrollBar, testCase, data.properties)

        mousePress(control, control.leftPadding + control.availableWidth * 0.5, control.topPadding + control.availableHeight * 0.5, Qt.LeftButton)
        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.topPadding + control.availableHeight * 0.5, Qt.LeftButton)

        compare(control.position, 0.5)
    }

    function test_warning() {
        ignoreWarning(Qt.resolvedUrl("tst_scrollbar.qml") + ":55:1: QML TestCase: ScrollBar must be attached to a Flickable or ScrollView")
        testCase.ScrollBar.vertical = null
    }

    function test_mirrored() {
        var container = createTemporaryObject(flickable, testCase)
        verify(container)
        waitForRendering(container)

        container.ScrollBar.vertical = scrollBar.createObject(container)
        compare(container.ScrollBar.vertical.x, container.width - container.ScrollBar.vertical.width)
        container.ScrollBar.vertical.locale = Qt.locale("ar_EG")
        compare(container.ScrollBar.vertical.x, 0)
    }

    function test_hover_data() {
        return [
            { tag: "enabled", hoverEnabled: true, interactive: true },
            { tag: "disabled", hoverEnabled: false, interactive: true },
            { tag: "non-interactive", hoverEnabled: true, interactive: false }
        ]
    }

    function test_hover(data) {
        var control = createTemporaryObject(scrollBar, testCase, {hoverEnabled: data.hoverEnabled, interactive: data.interactive})
        verify(control)

        compare(control.hovered, false)

        mouseMove(control)
        compare(control.hovered, data.hoverEnabled)
        compare(control.active, data.hoverEnabled && data.interactive)

        mouseMove(control, -1, -1)
        compare(control.hovered, false)
        compare(control.active, false)
    }

    function test_snapMode_data() {
        return [
            { tag: "NoSnap", snapMode: ScrollBar.NoSnap, stepSize: 0.1, size: 0.2, width: 100, steps: 80 }, /* 0.8*100 */
            { tag: "NoSnap2", snapMode: ScrollBar.NoSnap, stepSize: 0.2, size: 0.1, width: 200, steps: 180 }, /* 0.9*200 */

            { tag: "SnapAlways", snapMode: ScrollBar.SnapAlways, stepSize: 0.1, size: 0.2, width: 100, steps: 10 },
            { tag: "SnapAlways2", snapMode: ScrollBar.SnapAlways, stepSize: 0.2, size: 0.1, width: 200, steps: 5 },

            { tag: "SnapOnRelease", snapMode: ScrollBar.SnapOnRelease, stepSize: 0.1, size: 0.2, width: 100, steps: 80 }, /* 0.8*100 */
            { tag: "SnapOnRelease2", snapMode: ScrollBar.SnapOnRelease, stepSize: 0.2, size: 0.1, width: 200, steps: 180 }, /* 0.9*200 */
        ]
    }

    function test_snapMode_mouse_data() {
        return test_snapMode_data()
    }

    function test_snapMode_mouse(data) {
        var control = createTemporaryObject(scrollBar, testCase, {snapMode: data.snapMode, orientation: Qt.Horizontal, stepSize: data.stepSize, size: data.size, width: data.width})
        verify(control)

        function snappedPosition(pos) {
            var effectiveStep = control.stepSize * (1.0 - control.size)
            return Math.round(pos / effectiveStep) * effectiveStep
        }

        function boundPosition(pos) {
            return Math.max(0, Math.min(pos, 1.0 - control.size))
        }

        mousePress(control, 0, 0)
        compare(control.position, 0)

        mouseMove(control, control.width * 0.3, 0)
        var expectedMovePos = 0.3
        if (control.snapMode === ScrollBar.SnapAlways) {
            expectedMovePos = snappedPosition(expectedMovePos)
            verify(expectedMovePos !== 0.3)
        }
        compare(control.position, expectedMovePos)

        mouseRelease(control, control.width * 0.75, 0)
        var expectedReleasePos = 0.75
        if (control.snapMode !== ScrollBar.NoSnap) {
            expectedReleasePos = snappedPosition(expectedReleasePos)
            verify(expectedReleasePos !== 0.75)
        }
        compare(control.position, expectedReleasePos)

        control.position = 0
        mousePress(control, 0, 0)

        var steps = 0
        var prevPos = 0

        for (var x = 0; x < control.width; ++x) {
            mouseMove(control, x, 0)
            expectedMovePos = boundPosition(x / control.width)
            if (control.snapMode === ScrollBar.SnapAlways)
                expectedMovePos = snappedPosition(expectedMovePos)
            compare(control.position, expectedMovePos)

            if (control.position !== prevPos)
                ++steps
            prevPos = control.position
        }
        compare(steps, data.steps)

        mouseRelease(control, control.width - 1, 0)
    }

    function test_snapMode_touch_data() {
        return test_snapMode_data()
    }

    function test_snapMode_touch(data) {
        var control = createTemporaryObject(scrollBar, testCase, {snapMode: data.snapMode, orientation: Qt.Horizontal, stepSize: data.stepSize, size: data.size, width: data.width})
        verify(control)

        function snappedPosition(pos) {
            var effectiveStep = control.stepSize * (1.0 - control.size)
            return Math.round(pos / effectiveStep) * effectiveStep
        }

        function boundPosition(pos) {
            return Math.max(0, Math.min(pos, 1.0 - control.size))
        }

        var touch = touchEvent(control)

        touch.press(0, control, 0, 0).commit()
        compare(control.position, 0)

        touch.move(0, control, control.width * 0.3, 0).commit()
        var expectedMovePos = 0.3
        if (control.snapMode === ScrollBar.SnapAlways) {
            expectedMovePos = snappedPosition(expectedMovePos)
            verify(expectedMovePos !== 0.3)
        }
        compare(control.position, expectedMovePos)

        touch.release(0, control, control.width * 0.75, 0).commit()
        var expectedReleasePos = 0.75
        if (control.snapMode !== ScrollBar.NoSnap) {
            expectedReleasePos = snappedPosition(expectedReleasePos)
            verify(expectedReleasePos !== 0.75)
        }
        compare(control.position, expectedReleasePos)

        control.position = 0
        touch.press(0, control, 0, 0).commit()

        var steps = 0
        var prevPos = 0

        for (var x = 0; x < control.width; ++x) {
            touch.move(0, control, x, 0).commit()
            expectedMovePos = boundPosition(x / control.width)
            if (control.snapMode === ScrollBar.SnapAlways)
                expectedMovePos = snappedPosition(expectedMovePos)
            compare(control.position, expectedMovePos)

            if (control.position !== prevPos)
                ++steps
            prevPos = control.position
        }
        compare(steps, data.steps)

        touch.release(0, control, control.width - 1).commit()
    }

    function test_interactive_data() {
        return [
            { tag: "true", interactive: true },
            { tag: "false", interactive: false }
        ]
    }

    function test_interactive(data) {
        var control = createTemporaryObject(scrollBar, testCase, {interactive: data.interactive})
        verify(control)

        compare(control.interactive, data.interactive)

        // press-move-release
        mousePress(control, 0, 0, Qt.LeftButton)
        compare(control.pressed, data.interactive)

        mouseMove(control, control.width / 2, control.height / 2)
        compare(control.position, data.interactive ? 0.5 : 0.0)

        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, false)

        // change to non-interactive while pressed
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, data.interactive)

        mouseMove(control, control.width, control.height)
        compare(control.position, data.interactive ? 1.0 : 0.0)

        control.interactive = false
        compare(control.interactive, false)
        compare(control.pressed, false)

        mouseMove(control, control.width / 2, control.height / 2)
        compare(control.position, data.interactive ? 1.0 : 0.0)

        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, false)

        // change back to interactive & try press-move-release again
        control.interactive = true
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)

        mouseMove(control, 0, 0)
        compare(control.position, 0.0)

        mouseRelease(control, 0, 0, Qt.LeftButton)
        compare(control.pressed, false)
    }

    function test_policy() {
        var control = createTemporaryObject(scrollBar, testCase, {active: true})
        verify(control)

        compare(ScrollBar.AsNeeded, Qt.ScrollBarAsNeeded)
        compare(ScrollBar.AlwaysOff, Qt.ScrollBarAlwaysOff)
        compare(ScrollBar.AlwaysOn, Qt.ScrollBarAlwaysOn)

        compare(control.visible, true)
        compare(control.policy, ScrollBar.AsNeeded)

        control.size = 0.5
        verify(control.state === "active" || control.contentItem.state === "active")

        control.size = 1.0
        verify(control.state !== "active" && control.contentItem.state !== "active")

        control.policy = ScrollBar.AlwaysOff
        compare(control.visible, false)

        control.policy = ScrollBar.AlwaysOn
        compare(control.visible, true)
        verify(control.state === "active" || control.contentItem.state === "active")
    }

    function test_overshoot() {
        var container = flickable.createObject(testCase)
        verify(container)
        waitForRendering(container)

        var vertical = scrollBar.createObject(container, {size: 0.5})
        container.ScrollBar.vertical = vertical

        var horizontal = scrollBar.createObject(container, {size: 0.5})
        container.ScrollBar.horizontal = horizontal

        // negative vertical overshoot (pos < 0)
        vertical.position = -0.1
        compare(vertical.contentItem.y, vertical.topPadding)
        compare(vertical.contentItem.height, 0.4 * vertical.availableHeight)

        // positive vertical overshoot (pos + size > 1)
        vertical.position = 0.8
        compare(vertical.contentItem.y, vertical.topPadding + 0.8 * vertical.availableHeight)
        compare(vertical.contentItem.height, 0.2 * vertical.availableHeight)

        // negative horizontal overshoot (pos < 0)
        horizontal.position = -0.1
        compare(horizontal.contentItem.x, horizontal.leftPadding)
        compare(horizontal.contentItem.width, 0.4 * horizontal.availableWidth)

        // positive horizontal overshoot (pos + size > 1)
        horizontal.position = 0.8
        compare(horizontal.contentItem.x, horizontal.leftPadding + 0.8 * horizontal.availableWidth)
        compare(horizontal.contentItem.width, 0.2 * horizontal.availableWidth)

        container.destroy()
    }

    function test_flashing() {
        var control = createTemporaryObject(scrollBar, testCase, {size: 0.2})
        verify(control)

        var activeSpy = signalSpy.createObject(control, {target: control, signalName: "activeChanged"})
        verify(activeSpy.valid)

        compare(control.active, false)
        if (control.contentItem)
            compare(control.contentItem.opacity, 0)
        if (control.background)
            compare(control.background.opacity, 0)

        control.increase()
        compare(control.position, 0.1)
        compare(control.active, false)
        compare(activeSpy.count, 2)
        if (control.contentItem)
            verify(control.contentItem.opacity > 0)
        if (control.background)
            verify(control.background.opacity > 0)
        if (control.contentItem)
            tryCompare(control.contentItem, "opacity", 0)
        if (control.background)
            tryCompare(control.background, "opacity", 0)

        control.decrease()
        compare(control.position, 0.0)
        compare(control.active, false)
        compare(activeSpy.count, 4)
        if (control.contentItem)
            verify(control.contentItem.opacity > 0)
        if (control.background)
            verify(control.background.opacity > 0)
        if (control.contentItem)
            tryCompare(control.contentItem, "opacity", 0)
        if (control.background)
            tryCompare(control.background, "opacity", 0)
    }
}
