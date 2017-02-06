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
    name: "ToolButton"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: toolButton
        ToolButton { }
    }

    function test_text() {
        var control = toolButton.createObject(testCase)
        verify(control)

        compare(control.text, "")
        control.text = "ToolButton"
        compare(control.text, "ToolButton")
        control.text = ""
        compare(control.text, "")

        control.destroy()
    }

    function test_mouse() {
        var control = toolButton.createObject(testCase)
        verify(control)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        var downSpy = signalSpy.createObject(control, {target: control, signalName: "downChanged"})
        verify(downSpy.valid)

        var clickedSpy = signalSpy.createObject(control, {target: control, signalName: "clicked"})
        verify(clickedSpy.valid)

        // check
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftToolButton)
        compare(pressedSpy.count, 1)
        compare(downSpy.count, 1)
        compare(control.pressed, true)
        compare(control.down, true)
        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftToolButton)
        compare(clickedSpy.count, 1)
        compare(pressedSpy.count, 2)
        compare(downSpy.count, 2)
        compare(control.pressed, false)
        compare(control.down, false)

        // uncheck
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftToolButton)
        compare(pressedSpy.count, 3)
        compare(downSpy.count, 3)
        compare(control.pressed, true)
        compare(control.down, true)
        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftToolButton)
        compare(clickedSpy.count, 2)
        compare(pressedSpy.count, 4)
        compare(downSpy.count, 4)
        compare(control.pressed, false)
        compare(control.down, false)

        // release outside
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftToolButton)
        compare(pressedSpy.count, 5)
        compare(downSpy.count, 5)
        compare(control.pressed, true)
        compare(control.down, true)
        mouseMove(control, control.width * 2, control.height * 2, 0, Qt.LeftToolButton)
        compare(control.pressed, false)
        compare(control.down, false)
        mouseRelease(control, control.width * 2, control.height * 2, Qt.LeftToolButton)
        compare(clickedSpy.count, 2)
        compare(pressedSpy.count, 6)
        compare(downSpy.count, 6)
        compare(control.pressed, false)
        compare(control.down, false)

        // right button
        mousePress(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(pressedSpy.count, 6)
        compare(downSpy.count, 6)
        compare(control.pressed, false)
        compare(control.down, false)
        mouseRelease(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(clickedSpy.count, 2)
        compare(pressedSpy.count, 6)
        compare(downSpy.count, 6)
        compare(control.pressed, false)
        compare(control.down, false)

        control.destroy()
    }

    function test_keys() {
        var control = toolButton.createObject(testCase)
        verify(control)

        var clickedSpy = signalSpy.createObject(control, {target: control, signalName: "clicked"})
        verify(clickedSpy.valid)

        control.forceActiveFocus()
        verify(control.activeFocus)

        // check
        keyClick(Qt.Key_Space)
        compare(clickedSpy.count, 1)

        // uncheck
        keyClick(Qt.Key_Space)
        compare(clickedSpy.count, 2)

        // no change
        var keys = [Qt.Key_Enter, Qt.Key_Return, Qt.Key_Escape, Qt.Key_Tab]
        for (var i = 0; i < keys.length; ++i) {
            keyClick(keys[i])
            compare(clickedSpy.count, 2)
        }

        control.destroy()
    }

    function test_baseline() {
        var control = toolButton.createObject(testCase)
        verify(control)
        compare(control.baselineOffset, control.contentItem.y + control.contentItem.baselineOffset)
        control.destroy()
    }
}
