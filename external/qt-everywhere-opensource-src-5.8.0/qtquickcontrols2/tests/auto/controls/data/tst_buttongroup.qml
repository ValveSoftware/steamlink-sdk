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
    name: "ButtonGroup"

    Component {
        id: buttonGroup
        ButtonGroup { }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_null() {
        var group = buttonGroup.createObject(testCase)
        verify(group)

        group.addButton(null)
        group.removeButton(null)

        group.destroy()
    }

    Component {
        id: button
        Button { }
    }

    Component {
        id: nonCheckable
        QtObject { }
    }

    function test_current() {
        var group = buttonGroup.createObject(testCase)
        verify(group)

        var checkedButtonSpy = signalSpy.createObject(testCase, {target: group, signalName: "checkedButtonChanged"})
        verify(checkedButtonSpy.valid)
        verify(!group.checkedButton)

        var button1 = button.createObject(testCase, {checked: true})
        var button2 = button.createObject(testCase, {checked: false})
        var button3 = button.createObject(testCase, {checked: true, objectName: "3"})

        // add checked
        group.addButton(button1)
        compare(group.checkedButton, button1)
        compare(button1.checked, true)
        compare(button2.checked, false)
        compare(button3.checked, true)
        compare(checkedButtonSpy.count, 1)

        // add non-checked
        group.addButton(button2)
        compare(group.checkedButton, button1)
        compare(button1.checked, true)
        compare(button2.checked, false)
        compare(button3.checked, true)
        compare(checkedButtonSpy.count, 1)

        // add checked
        group.addButton(button3)
        compare(group.checkedButton, button3)
        compare(button1.checked, false)
        compare(button2.checked, false)
        compare(button3.checked, true)
        compare(checkedButtonSpy.count, 2)

        // change current
        group.checkedButton = button2
        compare(group.checkedButton, button2)
        compare(button1.checked, false)
        compare(button2.checked, true)
        compare(button3.checked, false)
        compare(checkedButtonSpy.count, 3)

        // check
        button1.checked = true
        compare(group.checkedButton, button1)
        compare(button1.checked, true)
        compare(button2.checked, false)
        compare(button3.checked, false)
        compare(checkedButtonSpy.count, 4)

        // remove non-checked
        group.removeButton(button2)
        compare(group.checkedButton, button1)
        compare(button1.checked, true)
        compare(button2.checked, false)
        compare(button3.checked, false)
        compare(checkedButtonSpy.count, 4)

        // remove checked
        group.removeButton(button1)
        verify(!group.checkedButton)
        compare(button1.checked, false)
        compare(button2.checked, false)
        compare(button3.checked, false)
        compare(checkedButtonSpy.count, 5)

        group.destroy()
    }

    function test_buttons() {
        var group = buttonGroup.createObject(testCase)
        verify(group)

        var buttonsSpy = signalSpy.createObject(testCase, {target: group, signalName: "buttonsChanged"})
        verify(buttonsSpy.valid)

        compare(group.buttons.length, 0)
        compare(group.checkedButton, null)

        var button1 = button.createObject(testCase, {checked: true})
        var button2 = button.createObject(testCase, {checked: false})

        group.buttons = [button1, button2]
        compare(group.buttons.length, 2)
        compare(group.buttons[0], button1)
        compare(group.buttons[1], button2)
        compare(group.checkedButton, button1)
        compare(buttonsSpy.count, 2)

        var button3 = button.createObject(testCase, {checked: true})

        group.addButton(button3)
        compare(group.buttons.length, 3)
        compare(group.buttons[0], button1)
        compare(group.buttons[1], button2)
        compare(group.buttons[2], button3)
        compare(group.checkedButton, button3)
        compare(buttonsSpy.count, 3)

        group.removeButton(button1)
        compare(group.buttons.length, 2)
        compare(group.buttons[0], button2)
        compare(group.buttons[1], button3)
        compare(group.checkedButton, button3)
        compare(buttonsSpy.count, 4)

        group.buttons = []
        compare(group.buttons.length, 0)
        tryCompare(group, "checkedButton", null)
        compare(buttonsSpy.count, 5)

        group.destroy()
    }

    function test_clicked() {
        var group = buttonGroup.createObject(testCase)
        verify(group)

        var clickedSpy = signalSpy.createObject(testCase, {target: group, signalName: "clicked"})
        verify(clickedSpy.valid)

        var button1 = button.createObject(testCase)
        var button2 = button.createObject(testCase)

        group.addButton(button1)
        group.addButton(button2)

        button1.clicked()
        compare(clickedSpy.count, 1)
        compare(clickedSpy.signalArguments[0][0], button1)

        button2.clicked()
        compare(clickedSpy.count, 2)
        compare(clickedSpy.signalArguments[1][0], button2)

        group.destroy()
        button1.destroy()
        button2.destroy()
    }

    Component {
        id: checkBoxes
        Item {
            property ButtonGroup group: ButtonGroup { id: group }
            property CheckBox control1: CheckBox { ButtonGroup.group: group }
            property CheckBox control2: CheckBox { ButtonGroup.group: group }
            property CheckBox control3: CheckBox { ButtonGroup.group: group }
        }
    }

    Component {
        id: radioButtons
        Item {
            property ButtonGroup group: ButtonGroup { id: group }
            property RadioButton control1: RadioButton { ButtonGroup.group: group }
            property RadioButton control2: RadioButton { ButtonGroup.group: group }
            property RadioButton control3: RadioButton { ButtonGroup.group: group }
        }
    }

    Component {
        id: switches
        Item {
            property ButtonGroup group: ButtonGroup { id: group }
            property Switch control1: Switch { ButtonGroup.group: group }
            property Switch control2: Switch { ButtonGroup.group: group }
            property Switch control3: Switch { ButtonGroup.group: group }
        }
    }

    Component {
        id: childControls
        Item {
            id: container
            property ButtonGroup group: ButtonGroup { id: group; buttons: container.children }
            property alias control1: control1
            property alias control2: control2
            property alias control3: control3
            CheckBox { id: control1 }
            RadioButton { id: control2 }
            Switch { id: control3 }
        }
    }

    function test_controls_data() {
        return [
            { tag: "CheckBox", component: checkBoxes },
            { tag: "RadioButton", component: radioButtons },
            { tag: "Switch", component: switches },
            { tag: "Children", component: childControls }
        ]
    }

    function test_controls(data) {
        var container = data.component.createObject(testCase)
        verify(container)

        verify(!container.group.checkedButton)

        container.control1.checked = true
        compare(container.group.checkedButton, container.control1)
        compare(container.control1.checked, true)
        compare(container.control2.checked, false)
        compare(container.control3.checked, false)

        container.control2.checked = true
        compare(container.group.checkedButton, container.control2)
        compare(container.control1.checked, false)
        compare(container.control2.checked, true)
        compare(container.control3.checked, false)

        container.control3.checked = true
        compare(container.group.checkedButton, container.control3)
        compare(container.control1.checked, false)
        compare(container.control2.checked, false)
        compare(container.control3.checked, true)

        container.destroy()
    }

    function test_buttonDestroyed() {
        var group = buttonGroup.createObject(testCase)
        verify(group)

        var buttonsSpy = signalSpy.createObject(testCase, {target: group, signalName: "buttonsChanged"})
        verify(buttonsSpy.valid)

        var button1 = button.createObject(testCase, {objectName: "button1", checked: true})

        group.addButton(button1)
        compare(group.buttons.length, 1)
        compare(group.buttons[0], button1)
        compare(group.checkedButton, button1)
        compare(buttonsSpy.count, 1)

        button1.destroy()
        wait(0)
        compare(group.buttons.length, 0)
        compare(group.checkedButton, null)
        compare(buttonsSpy.count, 2)

        group.destroy()
    }

    Component {
        id: repeater
        Column {
            id: column
            property ButtonGroup group: ButtonGroup { buttons: column.children }
            property alias repeater: r
            Repeater {
                id: r
                model: 3
                delegate: RadioDelegate {
                    checked: index == 0
                    objectName: index
                }
            }
        }
    }

    function test_repeater() {
        var container = repeater.createObject(testCase)
        verify(container)

        verify(container.group.checkedButton)
        compare(container.group.checkedButton.objectName, "0")

        container.destroy()
    }
}
