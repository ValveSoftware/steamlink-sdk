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
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0

TestCase {
    id: testCase
    name: "Tests_Shortcut"
    when: windowShown
    width: 400
    height: 400

    property var rootObject

    function initTestCase() {
        var component = Qt.createComponent("shortcut/shortcuts.qml");
        compare(component.status, Component.Ready)
        rootObject =  component.createObject(testCase);
        verify(rootObject !== null, "created object is null")
        rootObject.forceActiveFocus();
    }

    function cleanup() {
        if (rootObject !== null)
            rootObject.destroy()
    }

    function test_shortcut_data() {
        return [
            { tag: "a_pressed", key: Qt.Key_A, modifier: Qt.NoModifier, expected: "a pressed" },
            { tag: "b_pressed", key: Qt.Key_B, modifier: Qt.NoModifier, expected: "b pressed" },
            { tag: "nokey_pressed1", key: Qt.Key_C, modifier: Qt.NoModifier, expected: "no key press" },
            { tag: "ctrlc_pressed", key: Qt.Key_C, modifier: Qt.ControlModifier, expected: "ctrl c pressed" },
            { tag: "dpressed", key: Qt.Key_D, modifier: Qt.NoModifier, expected: "d pressed" },
            { tag: "nokey_pressed2", key: Qt.Key_D, modifier: Qt.ControlModifier, expected: "no key press" },
            // shift d is not triggered because it is overloaded
            { tag: "overloaded", key: Qt.Key_D, modifier: Qt.ShiftModifier, expected: "no key press",
                        warning: "QQuickAction::event: Ambiguous shortcut overload: Shift+D" },
            { tag: "aldd_pressed", key: Qt.Key_D, modifier: Qt.AltModifier, expected: "alt d pressed" },
            { tag: "nokey_pressed3", key: Qt.Key_T, modifier: Qt.NoModifier, expected: "no key press" },
            // on mac we don't have mnemonics
            { tag: "mnemonics", key: Qt.Key_T, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt t pressed" },
            { tag: "checkbox", key: Qt.Key_C, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt c pressed" },
            { tag: "radiobutton", key: Qt.Key_R, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt r pressed" },
            { tag: "button", key: Qt.Key_1, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt 1 pressed" },
            { tag: "button+action", key: Qt.Key_2, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt 2 pressed" },
            { tag: "toolbutton", key: Qt.Key_3, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt 3 pressed" },
            { tag: "toolbutton+action", key: Qt.Key_4, modifier: Qt.AltModifier, expected: Qt.platform.os === "osx" ? "no key press" : "alt 4 pressed" },
        ]
    }

    function test_shortcut(data) {

        verify(rootObject != undefined);
        var text = rootObject.children[0];
        text.text = "no key press";

        if (data.warning !== undefined) {
            if (Qt.platform.os === "osx" && data.tag === "overloaded")
                ignoreWarning("QQuickAction::event: Ambiguous shortcut overload: ?D") // QTBUG_32089
            else
                ignoreWarning(data.warning)
        }
        keyPress(data.key, data.modifier);

        verify(text != undefined);
        compare(text.text, data.expected.toString());

    }
}



