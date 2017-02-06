/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
import Qt.labs.platform 1.0

TestCase {
    id: testCase
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "MenuItem"

    Component {
        id: menuItem
        MenuItem { }
    }

    SignalSpy {
        id: spy
    }

    function test_properties_data() {
        return [
            {tag: "enabled", signal: "enabledChanged", init: true, value: false},
            {tag: "visible", signal: "visibleChanged", init: true, value: false},
            {tag: "separator", signal: "separatorChanged", init: false, value: true},
            {tag: "checkable", signal: "checkableChanged", init: false, value: true},
            {tag: "checked", signal: "checkedChanged", init: false, value: true},
            {tag: "role", signal: "roleChanged", init: MenuItem.TextHeuristicRole, value: MenuItem.AboutRole},
            {tag: "text", signal: "textChanged", init: "", value: "text"},
            {tag: "iconSource", signal: "iconSourceChanged", init: "", value: "qrc:/undo.png"},
            {tag: "iconName", signal: "iconNameChanged", init: "", value: "edit-undo"},
            {tag: "shortcut", signal: "shortcutChanged", init: undefined, value: StandardKey.Undo}
        ]
    }

    function test_properties(data) {
        var item = menuItem.createObject(testCase)
        verify(item)

        spy.target = item
        spy.signalName = data.signal
        verify(spy.valid)

        compare(item[data.tag], data.init)
        item[data.tag] = data.value
        compare(spy.count, 1)
        compare(item[data.tag], data.value)

        item[data.tag] = data.value
        compare(spy.count, 1)

        spy.clear()
        item.destroy()
    }

    function test_role() {
        // Q_ENUMS(QPlatformMenuItem::MenuRole)
        compare(MenuItem.NoRole, 0)
        compare(MenuItem.TextHeuristicRole, 1)
        compare(MenuItem.ApplicationSpecificRole, 2)
        compare(MenuItem.AboutQtRole, 3)
        compare(MenuItem.AboutRole, 4)
        compare(MenuItem.PreferencesRole, 5)
        compare(MenuItem.QuitRole, 6)
    }
}
