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

import QtQuick 2.2
import QtTest 1.0
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 200
    height: 200

    TestCase {
        id: testCase
        name: "Tests_Switch"
        when: windowShown
        width: 200
        height: 200

        property var aSwitch

        SignalSpy {
            id: signalSpy
        }

        function init() {
            aSwitch = Qt.createQmlObject("import QtQuick.Controls 1.2; Switch { }", container, "");
        }

        function cleanup() {
            if (aSwitch !== null) {
                aSwitch.destroy()
            }
            signalSpy.clear();
        }

        function test_defaultConstructed() {
            compare(aSwitch.checked, false);
        }

        function test_checked() {
            signalSpy.signalName = "checkedChanged"
            signalSpy.target = aSwitch;
            compare(signalSpy.count, 0);

            compare(aSwitch.checked, false);
            aSwitch.checked = true;
            compare(aSwitch.checked, true);

            mouseClick(aSwitch, aSwitch.x + 1, aSwitch.y + 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(aSwitch.checked, true);

            // Clicking outside should do nothing.
            mouseClick(aSwitch, aSwitch.x - 1, aSwitch.y, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(aSwitch.checked, true);

            mouseClick(aSwitch, aSwitch.x, aSwitch.y - 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(aSwitch.checked, true);

            mouseClick(aSwitch, aSwitch.x - 1, aSwitch.y - 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
            compare(aSwitch.checked, true);
        }

        function test_pressed() {
            signalSpy.signalName = "pressedChanged"
            signalSpy.target = aSwitch;
            compare(signalSpy.count, 0);
            compare(aSwitch.pressed, false);

            mousePress(aSwitch, aSwitch.x + 1, aSwitch.y + 1, Qt.LeftButton);
            compare(aSwitch.pressed, true);
            compare(signalSpy.count, 1);

            mouseRelease(aSwitch, aSwitch.x + 1, aSwitch.y + 1, Qt.LeftButton);
            compare(aSwitch.pressed, false);
            compare(signalSpy.count, 2);
        }

        function test_clicked() {
            signalSpy.signalName = "clicked"
            signalSpy.target = aSwitch;
            compare(signalSpy.count, 0);

            mouseClick(aSwitch, aSwitch.x + 1, aSwitch.y + 1, Qt.LeftButton);
            compare(signalSpy.count, 1);

            // release outside -> no clicked()
            mousePress(aSwitch, aSwitch.x + 1, aSwitch.y + 1, Qt.LeftButton);
            mouseMove(aSwitch, aSwitch.x - 1, aSwitch.y - 1);
            mouseRelease(aSwitch, aSwitch.x - 1, aSwitch.y - 1, Qt.LeftButton);
            compare(signalSpy.count, 1);
        }

        function test_keyPressed() {
            aSwitch.forceActiveFocus();
            signalSpy.signalName = "checkedChanged";
            signalSpy.target = aSwitch;
            compare(signalSpy.count, 0);
            signalSpy.clear();
            aSwitch.forceActiveFocus()
            keyPress(Qt.Key_Space);
            keyRelease(Qt.Key_Space);
            compare(signalSpy.count, 1);
        }

        function test_exclusiveGroup() {
            var root = Qt.createQmlObject("import QtQuick 2.2; import QtQuick.Controls 1.2; \n"
                + "Row { \n"
                + "    property alias aSwitch1: aSwitch1 \n"
                + "    property alias aSwitch2: aSwitch2 \n"
                + "    property alias aSwitch3: aSwitch3 \n"
                + "    property alias group: group \n"
                + "    ExclusiveGroup { id: group } \n"
                + "    Switch { id: aSwitch1; checked: true; exclusiveGroup: group } \n"
                + "    Switch { id: aSwitch2; exclusiveGroup: group } \n"
                + "    Switch { id: aSwitch3; exclusiveGroup: group } \n"
                + "}", container, "");

            root.forceActiveFocus();
            signalSpy.signalName = "checkedChanged";
            signalSpy.target = root.aSwitch1;
            compare(signalSpy.count, 0);

            compare(root.aSwitch1.exclusiveGroup, root.group);
            compare(root.aSwitch2.exclusiveGroup, root.group);
            compare(root.aSwitch3.exclusiveGroup, root.group);

            compare(root.aSwitch1.checked, true);
            compare(root.aSwitch2.checked, false);
            compare(root.aSwitch3.checked, false);

            root.aSwitch2.forceActiveFocus();
            keyPress(Qt.Key_Space);
            compare(signalSpy.count, 1);

            compare(root.aSwitch1.checked, false);
            compare(root.aSwitch2.checked, true);
            compare(root.aSwitch3.checked, false);

            root.aSwitch3.forceActiveFocus();
            keyPress(Qt.Key_Space);
            compare(signalSpy.count, 1);

            compare(root.aSwitch1.checked, false);
            compare(root.aSwitch2.checked, false);
            compare(root.aSwitch3.checked, true);

            root.aSwitch3.forceActiveFocus();
            keyPress(Qt.Key_Space);

            compare(root.aSwitch1.checked, false);
            compare(root.aSwitch2.checked, false);
            compare(root.aSwitch3.checked, false);

            // Shouldn't be any warnings, since we're not setting a group.
            root.aSwitch1.exclusiveGroup = null;
            root.aSwitch2.exclusiveGroup = null;
            root.aSwitch3.exclusiveGroup = null;
        }

        function test_activeFocusOnPress(){
            aSwitch.activeFocusOnPress = false
            verify(!aSwitch.activeFocus)
            mouseClick(aSwitch, aSwitch.x + 1, aSwitch.y + 1)
            verify(!aSwitch.activeFocus)
            aSwitch.activeFocusOnPress = true
            verify(!aSwitch.activeFocus)
            mouseClick(aSwitch, aSwitch.x + 1, aSwitch.y + 1)
            verify(aSwitch.activeFocus)
        }

        function test_activeFocusOnTab() {
            aSwitch.destroy()
            wait(0) //QTBUG-30523 so processEvents is called

            if (Qt.styleHints.tabFocusBehavior != Qt.TabFocusAllControls)
                skip("This function doesn't support NOT iterating all.")

            var test_control = 'import QtQuick 2.2; \
            import QtQuick.Controls 1.2;            \
            Item {                                  \
                width: 200;                         \
                height: 200;                        \
                property alias control1: _control1; \
                property alias control2: _control2; \
                property alias control3: _control3; \
                Switch  {                           \
                    y: 20;                          \
                    id: _control1;                  \
                    activeFocusOnTab: true;         \
                }                                   \
                Switch  {                           \
                    y: 70;                          \
                    id: _control2;                  \
                    activeFocusOnTab: false;        \
                }                                   \
                Switch  {                           \
                    y: 120;                         \
                    id: _control3;                  \
                    activeFocusOnTab: true;         \
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
