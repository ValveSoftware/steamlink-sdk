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
    name: "RadioButton"

    Component {
        id: radioButton
        RadioButton { }
    }

    Component {
        id: signalSequenceSpy
        SignalSequenceSpy {
            signals: ["pressed", "released", "canceled", "clicked", "pressedChanged", "checkedChanged"]
        }
    }

    function test_text() {
        var control = radioButton.createObject(testCase)
        verify(control)

        compare(control.text, "")
        control.text = "RadioButton"
        compare(control.text, "RadioButton")
        control.text = ""
        compare(control.text, "")

        control.destroy()
    }

    function test_checked() {
        var control = radioButton.createObject(testCase)
        verify(control)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        sequenceSpy.expectedSequence = [] // No change expected
        compare(control.checked, false)
        verify(sequenceSpy.success)

        sequenceSpy.expectedSequence = ["checkedChanged"]
        control.checked = true
        compare(control.checked, true)
        verify(sequenceSpy.success)

        sequenceSpy.reset()
        control.checked = false
        compare(control.checked, false)
        verify(sequenceSpy.success)

        control.destroy()
    }

    function test_mouse() {
        var control = radioButton.createObject(testCase)
        verify(control)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        // check
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true, "checked": false }],
                                        "pressed"]
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)
        verify(sequenceSpy.success)
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": false, "checked": false }],
                                        ["checkedChanged", { "pressed": false, "checked": true }],
                                        "released",
                                        "clicked"]
        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.checked, true)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // attempt uncheck
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true, "checked": true }],
                                        "pressed"]
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)
        verify(sequenceSpy.success)
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": false, "checked": true }],
                                        "released",
                                        "clicked"]
        mouseRelease(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.checked, true)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // release outside
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true, "checked": true }],
                                        "pressed"]
        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.pressed, true)
        verify(sequenceSpy.success)
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": false, "checked": true }]]
        mouseMove(control, control.width * 2, control.height * 2, 0, Qt.LeftButton)
        compare(control.pressed, false)
        sequenceSpy.expectedSequence = [["canceled", { "pressed": false, "checked": true }]]
        mouseRelease(control, control.width * 2, control.height * 2, Qt.LeftButton)
        compare(control.checked, true)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        // right button
        sequenceSpy.expectedSequence = []
        mousePress(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(control.pressed, false)
        verify(sequenceSpy.success)
        mouseRelease(control, control.width / 2, control.height / 2, Qt.RightButton)
        compare(control.checked, true)
        compare(control.pressed, false)
        verify(sequenceSpy.success)

        control.destroy()
    }

    function test_keys() {
        var control = radioButton.createObject(testCase)
        verify(control)

        var sequenceSpy = signalSequenceSpy.createObject(control, {target: control})

        sequenceSpy.expectedSequence = []
        control.forceActiveFocus()
        verify(control.activeFocus)
        verify(sequenceSpy.success)

        // check
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true, "checked": false }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false, "checked": false }],
                                        ["checkedChanged", { "pressed": false, "checked": true }],
                                        "released",
                                        "clicked"]
        keyClick(Qt.Key_Space)
        compare(control.checked, true)
        verify(sequenceSpy.success)

        // attempt uncheck
        sequenceSpy.expectedSequence = [["pressedChanged", { "pressed": true, "checked": true }],
                                        "pressed",
                                        ["pressedChanged", { "pressed": false, "checked": true }],
                                        "released",
                                        "clicked"]
        keyClick(Qt.Key_Space)
        compare(control.checked, true)
        verify(sequenceSpy.success)

        // no change
        sequenceSpy.expectedSequence = []
        var keys = [Qt.Key_Enter, Qt.Key_Return, Qt.Key_Escape, Qt.Key_Tab]
        for (var i = 0; i < keys.length; ++i) {
            sequenceSpy.reset()
            keyClick(keys[i])
            compare(control.checked, true)
            verify(sequenceSpy.success)
        }

        control.destroy()
    }

    Component {
        id: twoRadioButtones
        Item {
            property RadioButton rb1: RadioButton { id: rb1 }
            property RadioButton rb2: RadioButton { id: rb2; checked: rb1.checked; enabled: false }
        }
    }

    function test_binding() {
        var container = twoRadioButtones.createObject(testCase)
        verify(container)

        compare(container.rb1.checked, false)
        compare(container.rb2.checked, false)

        container.rb1.checked = true
        compare(container.rb1.checked, true)
        compare(container.rb2.checked, true)

        container.rb1.checked = false
        compare(container.rb1.checked, false)
        compare(container.rb2.checked, false)

        container.destroy()
    }

    Component {
        id: radioButtonGroup
        Column {
            // auto-exclusive buttons behave as if they were in their own exclusive group
            RadioButton { }
            RadioButton { }

            // explicitly grouped buttons are only exclusive with each other, not with
            // auto-exclusive buttons, and the autoExclusive property is ignored
            ButtonGroup { id: eg }
            RadioButton { ButtonGroup.group: eg }
            RadioButton { ButtonGroup.group: eg; autoExclusive: false }

            ButtonGroup { id: eg2 }
            RadioButton { id: rb1; Component.onCompleted: eg2.addButton(rb1) }
            RadioButton { id: rb2; Component.onCompleted: eg2.addButton(rb2) }

            // non-exclusive buttons don't affect the others
            RadioButton { autoExclusive: false }
            RadioButton { autoExclusive: false }
        }
    }

    function test_autoExclusive() {
        var container = radioButtonGroup.createObject(testCase)
        compare(container.children.length, 8)

        var checkStates = [false, false, false, false, false, false, false, false]
        for (var i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[0].checked = true
        checkStates[0] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[1].checked = true
        checkStates[0] = false
        checkStates[1] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[2].checked = true
        checkStates[2] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[3].checked = true
        checkStates[2] = false
        checkStates[3] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[4].checked = true
        checkStates[4] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[5].checked = true
        checkStates[4] = false
        checkStates[5] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[6].checked = true
        checkStates[6] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[7].checked = true
        checkStates[7] = true
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.children[0].checked = true
        checkStates[0] = true
        checkStates[1] = false
        for (i = 0; i < 8; ++i)
            compare(container.children[i].checked, checkStates[i])

        container.destroy()
    }

    function test_baseline() {
        var control = radioButton.createObject(testCase)
        verify(control)
        compare(control.baselineOffset, control.contentItem.y + control.contentItem.baselineOffset)
        control.destroy()
    }
}
