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
    name: "Menu"

    Component {
        id: item
        MenuItem { }
    }

    Component {
        id: menu
        Menu { }
    }

    SignalSpy {
        id: itemsSpy
        signalName: "itemsChanged"
    }

    function init() {
        verify(!itemsSpy.target)
        compare(itemsSpy.count, 0)
    }

    function cleanup() {
        itemsSpy.target = null
        itemsSpy.clear()
    }

    function test_addRemove() {
        var control = menu.createObject(testCase)

        itemsSpy.target = control
        verify(itemsSpy.valid)

        control.addItem(item.createObject(control, {text: "1"}))
        compare(control.items.length, 1)
        compare(control.items[0].text, "1")
        compare(itemsSpy.count, 1)

        control.addItem(item.createObject(control, {text: "2"}))
        compare(control.items.length, 2)
        compare(control.items[0].text, "1")
        compare(control.items[1].text, "2")
        compare(itemsSpy.count, 2)

        control.insertItem(1, item.createObject(control, {text: "3"}))
        compare(control.items.length, 3)
        compare(control.items[0].text, "1")
        compare(control.items[1].text, "3")
        compare(control.items[2].text, "2")
        compare(itemsSpy.count, 3)

        control.insertItem(0, item.createObject(control, {text: "4"}))
        compare(control.items.length, 4)
        compare(control.items[0].text, "4")
        compare(control.items[1].text, "1")
        compare(control.items[2].text, "3")
        compare(control.items[3].text, "2")
        compare(itemsSpy.count, 4)

        control.insertItem(control.items.length, item.createObject(control, {text: "5"}))
        compare(control.items.length, 5)
        compare(control.items[0].text, "4")
        compare(control.items[1].text, "1")
        compare(control.items[2].text, "3")
        compare(control.items[3].text, "2")
        compare(control.items[4].text, "5")
        compare(itemsSpy.count, 5)

        control.removeItem(control.items[4])
        compare(control.items.length, 4)
        compare(control.items[0].text, "4")
        compare(control.items[1].text, "1")
        compare(control.items[2].text, "3")
        compare(control.items[3].text, "2")
        compare(itemsSpy.count, 6)

        control.removeItem(control.items[0])
        compare(control.items.length, 3)
        compare(control.items[0].text, "1")
        compare(control.items[1].text, "3")
        compare(control.items[2].text, "2")
        compare(itemsSpy.count, 7)

        control.removeItem(control.items[1])
        compare(control.items.length, 2)
        compare(control.items[0].text, "1")
        compare(control.items[1].text, "2")
        compare(itemsSpy.count, 8)

        control.removeItem(control.items[1])
        compare(control.items.length, 1)
        compare(control.items[0].text, "1")
        compare(itemsSpy.count, 9)

        control.removeItem(control.items[0])
        compare(control.items.length, 0)
        compare(itemsSpy.count, 10)

        control.destroy()
    }

    Component {
        id: contentMenu
        Menu {
            QtObject { objectName: "object" }
            MenuItem { objectName: "item1" }
            Timer { objectName: "timer" }
            MenuItem { objectName: "item2" }
            Component { MenuItem { } }
        }
    }

    function test_content() {
        var control = contentMenu.createObject(testCase)

        function compareObjectNames(content, names) {
            if (content.length !== names.length)
                return false
            for (var i = 0; i < names.length; ++i) {
                if (content[i].objectName !== names[i])
                    return false
            }
            return true
        }

        itemsSpy.target = control
        verify(itemsSpy.valid)

        verify(compareObjectNames(control.data, ["object", "item1", "timer", "item2", ""]))
        verify(compareObjectNames(control.items, ["item1", "item2"]))

        control.addItem(item.createObject(control, {objectName: "item3"}))
        verify(compareObjectNames(control.data, ["object", "item1", "timer", "item2", "", "item3"]))
        verify(compareObjectNames(control.items, ["item1", "item2", "item3"]))
        compare(itemsSpy.count, 1)

        control.insertItem(0, item.createObject(control, {objectName: "item4"}))
        verify(compareObjectNames(control.data, ["object", "item1", "timer", "item2", "", "item3", "item4"]))
        verify(compareObjectNames(control.items, ["item4", "item1", "item2", "item3"]))
        compare(itemsSpy.count, 2)

        control.removeItem(control.items[1])
        verify(compareObjectNames(control.data, ["object", "timer", "item2", "", "item3", "item4"]))
        verify(compareObjectNames(control.items, ["item4", "item2", "item3"]))
        compare(itemsSpy.count, 3)

        control.destroy()
    }

    Component {
        id: dynamicMenu
        Menu {
            id: dmenu
            MenuItem { text: "static" }
            Component.onCompleted: {
                addItem(item.createObject(dmenu, {text: "added"}))
                insertItem(0, item.createObject(dmenu, {text: "inserted"}))
            }
        }
    }

    function test_dynamic() {
        var control = dynamicMenu.createObject(testCase)

        // insertItem(), addItem(), and static MenuItem {}
        compare(control.items.length, 3)
        compare(control.items[0].text, "inserted")

        var dying = item.createObject(control, {text: "dying"})
        control.addItem(dying)
        compare(control.items.length, 4)
        compare(control.items[3].text, "dying")
        dying.destroy()
        wait(0)
        compare(control.items.length, 3)

        control.destroy()
    }

    function test_type() {
        // Q_ENUMS(QPlatformMenu::MenuType)
        compare(Menu.DefaultMenu, 0)
        compare(Menu.EditMenu, 1)
    }
}
