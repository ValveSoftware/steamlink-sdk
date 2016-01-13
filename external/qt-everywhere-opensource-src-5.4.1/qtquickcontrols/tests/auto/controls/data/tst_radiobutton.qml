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
    width: 400
    height: 400

    TestCase {
        id: testCase
        name: "Tests_RadioButton"
        when: windowShown
        width: 400
        height: 400

        property var radioButton

        SignalSpy {
            id: signalSpy
        }

        function init() {
            radioButton = Qt.createQmlObject('import QtQuick.Controls 1.2; RadioButton {}', container, '');
        }

        function cleanup() {
            if (radioButton !== null) {
                radioButton.destroy()
            }
            signalSpy.clear();
        }

        function test_createRadioButton() {
            compare(radioButton.checked, false);
            compare(radioButton.text, "");
        }

        function test_defaultConstructed() {
            compare(radioButton.checked, false);
            compare(radioButton.text, "");
        }

        function test_text() {
            compare(radioButton.text, "");

            radioButton.text = "Check me!";
            compare(radioButton.text, "Check me!");
        }

        function test_checked() {
            compare(radioButton.checked, false);

            radioButton.checked = true;
            compare(radioButton.checked, true);

            radioButton.checked = false;
            compare(radioButton.checked, false);
        }

        function test_clicked() {
            signalSpy.signalName = "clicked"
            signalSpy.target = radioButton;
            compare(signalSpy.count, 0);
            mouseClick(radioButton, radioButton.x, radioButton.y, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(radioButton.checked, true);

            // Clicking outside should do nothing.
            mouseClick(radioButton, radioButton.x - 1, radioButton.y, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(radioButton.checked, true);

            mouseClick(radioButton, radioButton.x, radioButton.y - 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(radioButton.checked, true);

            mouseClick(radioButton, radioButton.x - 1, radioButton.y - 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(radioButton.checked, true);
        }

        function test_keyPressed() {
            radioButton.forceActiveFocus();

            signalSpy.signalName = "clicked";
            signalSpy.target = radioButton;
            compare(signalSpy.count, 0);

            // Try cycling through checked and unchecked.
            var expectedStates = [true, false];
            expectedStates = expectedStates.concat(expectedStates, expectedStates, expectedStates);
            for (var i = 0; i < expectedStates.length; ++i) {
                keyPress(Qt.Key_Space);
                keyRelease(Qt.Key_Space);
                compare(signalSpy.count, i + 1);
                compare(radioButton.checked, expectedStates[i]);
            }
        }

        function test_exclusiveGroup() {
            var root = Qt.createQmlObject("import QtQuick 2.2; import QtQuick.Controls 1.2; \n"
                + "Row { \n"
                + "    property alias radioButton1: radioButton1 \n"
                + "    property alias radioButton2: radioButton2 \n"
                + "    property alias group: group \n"
                + "    ExclusiveGroup { id: group } \n"
                + "    RadioButton { id: radioButton1; checked: true; exclusiveGroup: group } \n"
                + "    RadioButton { id: radioButton2; exclusiveGroup: group } \n"
                + "}", container, "");

            compare(root.radioButton1.exclusiveGroup, root.group);
            compare(root.radioButton2.exclusiveGroup, root.group);
            compare(root.radioButton1.checked, true);
            compare(root.radioButton2.checked, false);

            root.forceActiveFocus();

            signalSpy.target = root.radioButton2;
            signalSpy.signalName = "clicked";
            compare(signalSpy.count, 0);

            mouseClick(root.radioButton2, root.radioButton2.x, root.radioButton2.y, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(root.radioButton1.checked, false);
            compare(root.radioButton2.checked, true);

            signalSpy.clear();
            signalSpy.target = root.radioButton1;
            signalSpy.signalName = "clicked";
            compare(signalSpy.count, 0);

            mouseClick(root.radioButton1, root.radioButton1.x, root.radioButton1.y, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(root.radioButton1.checked, true);
            compare(root.radioButton2.checked, false);
            root.destroy()
        }

        function test_activeFocusOnPress(){
            radioButton.activeFocusOnPress = false
            verify(!radioButton.activeFocus)
            mouseClick(radioButton, radioButton.x + 1, radioButton.y + 1)
            verify(!radioButton.activeFocus)
            radioButton.activeFocusOnPress = true
            verify(!radioButton.activeFocus)
            mouseClick(radioButton, radioButton.x + 1, radioButton.y + 1)
            verify(radioButton.activeFocus)
        }

        function test_activeFocusOnTab() {
            radioButton.destroy()
            wait(0) //QTBUG-30523 so processEvents is called

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
                RadioButton  {                      \
                    y: 20;                          \
                    id: _control1;                  \
                    activeFocusOnTab: true;         \
                    text: "control1"                \
                }                                   \
                RadioButton  {                      \
                    y: 70;                          \
                    id: _control2;                  \
                    activeFocusOnTab: false;        \
                    text: "control2"                \
                }                                   \
                RadioButton  {                      \
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
    }
}
