/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 300
    height: 300

TestCase {
    id: testCase
    name: "Tests_Button"
    when:windowShown
    width:400
    height:400

    function test_isDefault() {
        var tmp = Qt.createQmlObject('import QtQuick.Controls 1.2; Button {id: button1}', testCase, '');
        compare(tmp.isDefault, false);
        tmp.destroy()
    }

    function test_text() {
        var tmp1 = Qt.createQmlObject('import QtQuick.Controls 1.2; Button {id: button2_1}', testCase, '');
        compare(tmp1.text, "");
        tmp1.text = "Hello";
        compare(tmp1.text, "Hello");

        var tmp2 = Qt.createQmlObject('import QtQuick.Controls 1.2; Button {id: button2_2; text: "Hello"}', testCase, '');
        compare(tmp2.text, "Hello");
        tmp1.destroy()
        tmp2.destroy()
    }

    SignalSpy {
        id: clickSpy
        signalName: "clicked"
    }

    function test_action() {
        var test_actionStr =
           'import QtQuick 2.2;                     \
            import QtQuick.Controls 1.2;            \
            Item {                                  \
                property var testAction: Action {   \
                    id: testAction;                 \
                    text: "Action text"             \
                }                                   \
                                                    \
                property var button: Button {       \
                    id: button;                     \
                    action: testAction              \
                }                                   \
            }                                       '

        var tmp = Qt.createQmlObject(test_actionStr, testCase, '')
        compare(tmp.button.text, "Action text")

        tmp.testAction.text = "Action Joe"
        compare(tmp.button.text, "Action Joe")

        tmp.button.text = "G.I. Joe"
        compare(tmp.button.text, "G.I. Joe")
        compare(tmp.testAction.text, "Action Joe")

        clickSpy.target = tmp.button
        tmp.testAction.trigger()
        compare(clickSpy.count, 1)
        tmp.destroy()
    }

    function test_activeFocusOnPress(){
        var control = Qt.createQmlObject('import QtQuick.Controls 1.2; Button {x: 20; y: 20; width: 100; height: 50}', container, '')
        control.activeFocusOnPress = false
        verify(!control.activeFocus)
        mouseClick(control, 30, 30)
        verify(!control.activeFocus)
        control.activeFocusOnPress = true
        verify(!control.activeFocus)
        mouseClick(control, 30, 30)
        verify(control.activeFocus)
        control.destroy()
    }

    function test_activeFocusOnTab() {
        if (!SystemInfo.tabAllWidgets)
            skip("This function doesn't support NOT iterating all.")

        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        Item {                                  \
            width: 200;                         \
            height: 200;                        \
            property alias control1: _control1; \
            property alias control2: _control2; \
            property alias control3: _control3; \
            Button  {                           \
                y: 20;                          \
                id: _control1;                  \
                activeFocusOnTab: true;         \
                text: "control1"                \
            }                                   \
            Button  {                           \
                y: 70;                          \
                id: _control2;                  \
                activeFocusOnTab: false;        \
                text: "control2"                \
            }                                   \
            Button  {                           \
                y: 120;                         \
                id: _control3;                  \
                activeFocusOnTab: true;         \
                text: "control3"                \
            }                                   \
        }                                       '

        var control = Qt.createQmlObject(test_control, container, '')

        control.control1.forceActiveFocus()
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)

        control.control2.activeFocusOnTab = true
        control.control3.activeFocusOnTab = false
        keyPress(Qt.Key_Tab)
        verify(!control.control1.activeFocus)
        verify(control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!control.control1.activeFocus)
        verify(control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        control.destroy()
    }

    SignalSpy {
        id: checkSpy
        signalName: "checkedChanged"
    }

    function test_checked() {
        var button = Qt.createQmlObject('import QtQuick.Controls 1.2; Button { checkable: true }', container, '')

        var checkCount = 0

        checkSpy.clear()
        checkSpy.target = button
        verify(checkSpy.valid)
        verify(!button.checked)

        // stays unchecked on press
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, checkCount)

        // gets checked on release inside
        mouseRelease(button, button.width / 2, button.height / 2)
        verify(!button.pressed)
        verify(button.checked)
        compare(checkSpy.count, ++checkCount)

        // stays checked on press
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        verify(button.checked)
        compare(checkSpy.count, checkCount)

        // stays checked on release outside
        mouseMove(button, button.width * 2, button.height * 2)
        mouseRelease(button, button.width * 2, button.height * 2)
        verify(!button.pressed)
        verify(button.checked)
        compare(checkSpy.count, checkCount)

        // stays checked on press
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        verify(button.checked)
        compare(checkSpy.count, checkCount)

        // gets unchecked on release inside
        mouseRelease(button, button.width / 2, button.height / 2)
        verify(!button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, ++checkCount)

        // stays unchecked on press
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, checkCount)

        // stays unchecked on release outside
        mouseMove(button, button.width * 2, button.height * 2)
        mouseRelease(button, button.width * 2, button.height * 2)
        verify(!button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, checkCount)

        // keyboard toggle
        button.forceActiveFocus()
        keyClick(Qt.Key_Space)
        verify(button.checked)
        compare(checkSpy.count, ++checkCount)

        // toggle on release
        keyPress(Qt.Key_Space)
        verify(button.checked)
        keyRelease(Qt.Key_Space)
        verify(!button.checked)
        compare(checkSpy.count, ++checkCount)

        button.destroy()
    }
}
}
