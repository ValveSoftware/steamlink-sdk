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
import QtQuick.Controls 1.2
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 400
    height: 400

TestCase {
    id: testCase
    name: "Tests_GroupBox"
    when:windowShown
    width:400
    height:400

    property var groupBox;

    Component {
        id: groupboxComponent
        GroupBox {
            property alias child1: child1
            property alias child2: child2
            Column {
                CheckBox {
                    id: child1
                    text: "checkbox1"
                }
                CheckBox {
                    id: child2
                    text: "checkbox2"
                }
            }
        }
    }

    function init() {
        groupBox = groupboxComponent.createObject(testCase)
    }

    function cleanup() {
        if (groupBox !== 0)
            groupBox.destroy()
    }

    function test_contentItem() {
        verify (groupBox.contentItem !== null)
        verify (groupBox.contentItem.anchors !== undefined)
    }

    function test_dynamicSize() {

        var groupbox = Qt.createQmlObject('import QtQuick.Controls 1.2; import QtQuick.Controls.Private 1.0 ; GroupBox {style:GroupBoxStyle{}}', container, '')
        compare(groupbox.width, 16)
        compare(groupbox.height, 16)

        var content = Qt.createQmlObject('import QtQuick 2.2; Rectangle {implicitWidth:100 ; implicitHeight:30}', container, '')
        content.parent = groupbox.contentItem
        compare(groupbox.implicitWidth, 116)
        compare(groupbox.implicitHeight, 46)
        content.parent = null
        content.destroy()

        content = Qt.createQmlObject('import QtQuick 2.2; Rectangle {width:20 ; height:20}', container, '')
        content.parent = groupbox.contentItem
        compare(groupbox.implicitWidth, 36)
        compare(groupbox.implicitHeight, 36)
        content.parent = null
        content.destroy()

        groupbox.destroy()
    }

    function test_checkable() {
        compare(groupBox.checkable, false)
        compare(groupBox.child1.enabled, true)
        compare(groupBox.child2.enabled, true)

        groupBox.checkable = true
        compare(groupBox.checkable, true)
        compare(groupBox.checked, true)
        compare(groupBox.child1.enabled, true)
        compare(groupBox.child2.enabled, true)

        groupBox.checkable = false
        compare(groupBox.child1.enabled, true)
        compare(groupBox.child2.enabled, true)
    }

    function test_checked() {
        groupBox.checkable = true
        groupBox.checked = false
        compare(groupBox.checked, false)
        compare(groupBox.child1.enabled, false)
        compare(groupBox.child2.enabled, false)

        groupBox.checked = true
        compare(groupBox.checked, true)
        compare(groupBox.child1.enabled, true)
        compare(groupBox.child2.enabled, true)
    }

    function test_activeFocusOnTab() {
        if (Qt.styleHints.tabFocusBehavior != Qt.TabFocusAllControls)
            skip("This function doesn't support NOT iterating all.")

        var component = Qt.createComponent("groupbox/gb_activeFocusOnTab.qml")
        compare(component.status, Component.Ready)
        var control =  component.createObject(container);
        verify(control !== null, "test control created is null")

        var gp1 = control.control1
        var gp2 = control.control2
        verify(gp1 !== null)
        verify(gp2 !== null)

        var check = getCheckBoxItem(gp1)
        verify(check !== null)

        var column1 = getColumnItem(gp1, "column1")
        verify(column1 !== null)
        var column2 = getColumnItem(gp2, "column2")
        verify(column2 !== null)

        var child1 = column1.child1
        verify(child1 !== null)
        var child2 = column1.child2
        verify(child2 !== null)
        var child3 = column2.child3
        verify(child3 !== null)
        var child4 = column2.child4
        verify(child4 !== null)

        control.forceActiveFocus()
        keyPress(Qt.Key_Tab)
        verify(check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!check.activeFocus)
        verify(child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(child4.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!check.activeFocus)
        verify(child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(!child4.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!check.activeFocus)
        verify(!child1.activeFocus)
        verify(!child2.activeFocus)
        verify(!child3.activeFocus)
        verify(child4.activeFocus)

        control.destroy()
    }

    function getCheckBoxItem(control) {
        for (var i = 0; i < control.children.length; i++) {
            if (control.children[i].objectName === 'check')
                return control.children[i]
        }
        return null
    }

    function getColumnItem(control, name) {
        for (var i = 0; i < control.children.length; i++) {
            if (control.children[i].objectName === 'container') {
                var sub = control.children[i]
                for (var j = 0; j < sub.children.length; j++) {
                    if (sub.children[j].objectName === name)
                        return sub.children[j]
                }
            }
        }
        return null
    }
}
}
