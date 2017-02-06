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
    name: "Button"

    Component {
        id: button
        Button { }
    }

    Component {
        id: signalSequenceSpy
        SignalSequenceSpy {
            signals: ["pressed", "released", "canceled", "clicked", "doubleClicked", "pressedChanged", "downChanged", "checkedChanged"]
        }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_text() {
        var control = button.createObject(testCase)
        verify(control)

        compare(control.text, "")
        control.text = "Button"
        compare(control.text, "Button")
        control.text = ""
        compare(control.text, "")

        control.destroy()
    }

    function test_mouse() {
        var control = button.createObject(testCase)
        verify(control)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        // click
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed"]
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        "released",
                                        "clicked"]
        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // release outside
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed"]
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }]]
        mouseMove(control, control.width * 2, control.height * 2, 0, Qt.LeftButton)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = [["canceled", { "pressed": false }]]
        mouseRelease(control, control.width * 2, control.height * 2, Qt.LeftButton)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // right button
        sequenceSpy.expectedSequence = []
        mousePress(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(control.pressed, false)

        mouseRelease(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // double click
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        "released",
                                        "clicked",
                                        ["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        "doubleClicked",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        "released",
                                        "clicked"]
        mouseDoubleClickSequence(control, control.width / 2, control.height / 2, Qt.LeftButton)
        verify(sequenceSpy.success)

        control.destroy()
    }

    function test_keys() {
        var control = button.createObject(testCase)
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        // click
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        "released",
                                        "clicked"]
        keyClick(Qt.Key_Space)
        verify(sequenceSpy.success)

        // no change
        sequenceSpy.expectedSequence = []
        var keys = [Qt.Key_Enter, Qt.Key_Return, Qt.Key_Escape, Qt.Key_Tab]
        for (var i = 0; i < keys.length; ++i) {
            sequenceSpy.reset()
            keyClick(keys[i])
            verify(sequenceSpy.success)
        }

        control.destroy()
    }

    function eventErrorMessage(actual, expected) {
        return "actual event:" + JSON.stringify(actual) + ", expected event:" + JSON.stringify(expected)
    }

    function test_autoRepeat() {
        var control = button.createObject(testCase)
        verify(control)

        compare(control.autoRepeat, false)
        control.autoRepeat = true
        compare(control.autoRepeat, true)

        control.forceActiveFocus()
        verify(control.activeFocus)

        var clickSpy = signalSpy.createObject(control, {target: control, signalName: "clicked"})
        verify(clickSpy.valid)
        var pressSpy = signalSpy.createObject(control, {target: control, signalName: "pressed"})
        verify(pressSpy.valid)
        var releaseSpy = signalSpy.createObject(control, {target: control, signalName: "released"})
        verify(releaseSpy.valid)

        // auto-repeat mouse click
        mousePress(control)
        compare(control.pressed, true)
        clickSpy.wait()
        clickSpy.wait()
        compare(pressSpy.count, clickSpy.count + 1)
        compare(releaseSpy.count, clickSpy.count)
        mouseRelease(control)
        compare(control.pressed, false)
        compare(clickSpy.count, pressSpy.count)
        compare(releaseSpy.count, pressSpy.count)

        clickSpy.clear()
        pressSpy.clear()
        releaseSpy.clear()

        // auto-repeat key click
        keyPress(Qt.Key_Space)
        compare(control.pressed, true)
        clickSpy.wait()
        clickSpy.wait()
        compare(pressSpy.count, clickSpy.count + 1)
        compare(releaseSpy.count, clickSpy.count)
        keyRelease(Qt.Key_Space)
        compare(control.pressed, false)
        compare(clickSpy.count, pressSpy.count)
        compare(releaseSpy.count, pressSpy.count)

        clickSpy.clear()
        pressSpy.clear()
        releaseSpy.clear()

        mousePress(control)
        compare(control.pressed, true)
        clickSpy.wait()
        compare(pressSpy.count, clickSpy.count + 1)
        compare(releaseSpy.count, clickSpy.count)

        // move inside during repeat -> continue repeat
        mouseMove(control, control.width / 4, control.height / 4)
        clickSpy.wait()
        compare(pressSpy.count, clickSpy.count + 1)
        compare(releaseSpy.count, clickSpy.count)

        clickSpy.clear()
        pressSpy.clear()
        releaseSpy.clear()

        // move outside during repeat -> stop repeat
        mouseMove(control, -1, -1)
        // NOTE: The following wait() is NOT a reliable way to test that the
        // auto-repeat timer is not running, but there's no way dig into the
        // private APIs from QML. If this test ever fails in the future, it
        // indicates that the auto-repeat timer logic is broken.
        wait(125)
        compare(clickSpy.count, 0)
        compare(pressSpy.count, 0)
        compare(releaseSpy.count, 0)

        mouseRelease(control, -1, -1)
        compare(control.pressed, false)
        compare(clickSpy.count, 0)
        compare(pressSpy.count, 0)
        compare(releaseSpy.count, 0)

        control.destroy()
    }

    function test_baseline() {
        var control = button.createObject(testCase)
        verify(control)
        compare(control.baselineOffset, control.contentItem.y + control.contentItem.baselineOffset)
        control.destroy()
    }

    function test_checkable() {
        var control = button.createObject(testCase)
        verify(control)
        verify(control.hasOwnProperty("checkable"))
        verify(!control.checkable)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        "released",
                                        "clicked"]
        mouseClick(control)
        verify(!control.checked)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        ["checkedChanged", { "checked": true }],
                                        "released",
                                        "clicked"]
        control.checkable = true
        mouseClick(control)
        verify(control.checked)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true }],
                                        ["downChanged", { "down": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false }],
                                        ["downChanged", { "down": false }],
                                        ["checkedChanged", { "checked": false }],
                                        "released",
                                        "clicked"]
        mouseClick(control)
        verify(!control.checked)
        verify(sequenceSpy.success)

        control.destroy()
    }

    function test_highlighted() {
        var control = button.createObject(testCase)
        verify(control)
        verify(!control.highlighted)

        control.highlighted = true
        verify(control.highlighted)

        control.destroy()
    }
}
