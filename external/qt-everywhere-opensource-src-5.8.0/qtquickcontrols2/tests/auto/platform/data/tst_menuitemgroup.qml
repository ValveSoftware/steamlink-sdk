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
    name: "MenuItemGroup"

    Component {
        id: menuItemGroup
        MenuItemGroup { }
    }

    SignalSpy {
        id: checkedItemSpy
        signalName: "checkedItemChanged"
    }

    SignalSpy {
        id: itemsSpy
        signalName: "itemsChanged"
    }

    function init() {
        verify(!checkedItemSpy.target)
        compare(checkedItemSpy.count, 0)

        verify(!itemsSpy.target)
        compare(itemsSpy.count, 0)
    }

    function cleanup() {
        checkedItemSpy.target = null
        checkedItemSpy.clear()

        itemsSpy.target = null
        itemsSpy.clear()
    }

    function test_null() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        group.addItem(null)
        group.removeItem(null)

        group.destroy()
    }

    Component {
        id: item
        MenuItem { }
    }

    function test_exclusive() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        compare(group.exclusive, true)

        checkedItemSpy.target = group
        verify(checkedItemSpy.valid)
        verify(!group.checkedItem)

        var item1 = item.createObject(testCase, {checked: true})
        var item2 = item.createObject(testCase, {checked: false})
        var item3 = item.createObject(testCase, {checked: true})

        // add checked
        group.addItem(item1)
        compare(group.checkedItem, item1)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 1)

        // add non-checked
        group.addItem(item2)
        compare(group.checkedItem, item1)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 1)

        // add checked
        group.addItem(item3)
        compare(group.checkedItem, item3)
        compare(item1.checked, false)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 2)

        // change checked
        group.checkedItem = item2
        compare(group.checkedItem, item2)
        compare(item1.checked, false)
        compare(item2.checked, true)
        compare(item3.checked, false)
        compare(checkedItemSpy.count, 3)

        // check
        item1.checked = true
        compare(group.checkedItem, item1)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, false)
        compare(checkedItemSpy.count, 4)

        // remove non-checked
        group.removeItem(item2)
        compare(group.checkedItem, item1)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, false)
        compare(checkedItemSpy.count, 4)

        // remove checked
        group.removeItem(item1)
        compare(group.checkedItem, null)
        compare(item1.checked, false)
        compare(item2.checked, false)
        compare(item3.checked, false)
        compare(checkedItemSpy.count, 5)

        group.destroy()
    }

    function test_nonExclusive() {
        var group = menuItemGroup.createObject(testCase, {exclusive: false})
        verify(group)

        compare(group.exclusive, false)

        checkedItemSpy.target = group
        verify(checkedItemSpy.valid)
        verify(!group.checkedItem)

        var item1 = item.createObject(testCase, {checked: true})
        var item2 = item.createObject(testCase, {checked: false})
        var item3 = item.createObject(testCase, {checked: true})

        // add checked
        group.addItem(item1)
        compare(group.checkedItem, null)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 0)

        // add non-checked
        group.addItem(item2)
        compare(group.checkedItem, null)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 0)

        // add checked
        group.addItem(item3)
        compare(group.checkedItem, null)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 0)

        // change checked
        group.checkedItem = item2
        compare(group.checkedItem, item2)
        compare(item1.checked, true)
        compare(item2.checked, true)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 1)

        // check
        item1.checked = false
        item1.checked = true
        compare(group.checkedItem, item2)
        compare(item1.checked, true)
        compare(item2.checked, true)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 1)

        // remove checked
        group.removeItem(item2)
        compare(group.checkedItem, null)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 2)

        // remove non-checked
        group.removeItem(item1)
        compare(group.checkedItem, null)
        compare(item1.checked, true)
        compare(item2.checked, false)
        compare(item3.checked, true)
        compare(checkedItemSpy.count, 2)

        group.destroy()
    }

    function test_items() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        itemsSpy.target = group
        verify(itemsSpy.valid)

        compare(group.items.length, 0)
        compare(group.checkedItem, null)

        var item1 = item.createObject(testCase, {checked: true})
        var item2 = item.createObject(testCase, {checked: false})

        group.items = [item1, item2]
        compare(group.items.length, 2)
        compare(group.items[0], item1)
        compare(group.items[1], item2)
        compare(group.checkedItem, item1)
        compare(itemsSpy.count, 2)

        var item3 = item.createObject(testCase, {checked: true})

        group.addItem(item3)
        compare(group.items.length, 3)
        compare(group.items[0], item1)
        compare(group.items[1], item2)
        compare(group.items[2], item3)
        compare(group.checkedItem, item3)
        compare(itemsSpy.count, 3)

        group.removeItem(item1)
        compare(group.items.length, 2)
        compare(group.items[0], item2)
        compare(group.items[1], item3)
        compare(group.checkedItem, item3)
        compare(itemsSpy.count, 4)

        group.items = []
        compare(group.items.length, 0)
        compare(group.checkedItem, null)
        compare(itemsSpy.count, 5)

        group.destroy()
    }

    function test_itemDestroyed() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        itemsSpy.target = group
        verify(itemsSpy.valid)

        var item1 = item.createObject(testCase, {checked: true})

        group.addItem(item1)
        compare(group.items.length, 1)
        compare(group.items[0], item1)
        compare(group.checkedItem, item1)
        compare(itemsSpy.count, 1)

        item1.destroy()
        wait(0)
        compare(group.items.length, 0)
        compare(group.checkedItem, null)
        compare(itemsSpy.count, 2)

        group.destroy()
    }

    function test_visible() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        compare(group.visible, true)

        for (var i = 0; i < 3; ++i) {
            group.addItem(item.createObject(testCase))
            compare(group.items[i].visible, true)
        }

        group.visible = false
        compare(group.visible, false)

        for (i = 0; i < 3; ++i)
            compare(group.items[i].visible, false)

        group.items[1].visible = true
        compare(group.items[1].visible, false)

        group.items[1].visible = false
        compare(group.items[1].visible, false)

        group.visible = true
        compare(group.visible, true)

        compare(group.items[0].visible, true)
        compare(group.items[1].visible, false)
        compare(group.items[2].visible, true)

        group.destroy()
    }

    function test_enabled() {
        var group = menuItemGroup.createObject(testCase)
        verify(group)

        compare(group.enabled, true)

        for (var i = 0; i < 3; ++i) {
            group.addItem(item.createObject(testCase))
            compare(group.items[i].enabled, true)
        }

        group.enabled = false
        compare(group.enabled, false)

        for (i = 0; i < 3; ++i)
            compare(group.items[i].enabled, false)

        group.items[1].enabled = true
        compare(group.items[1].enabled, false)

        group.items[1].enabled = false
        compare(group.items[1].enabled, false)

        group.enabled = true
        compare(group.enabled, true)

        compare(group.items[0].enabled, true)
        compare(group.items[1].enabled, false)
        compare(group.items[2].enabled, true)

        group.destroy()
    }
}
