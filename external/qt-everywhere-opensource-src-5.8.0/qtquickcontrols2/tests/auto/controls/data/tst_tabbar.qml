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
    name: "TabBar"

    Component {
        id: tabButton
        TabButton { }
    }

    Component {
        id: tabBar
        TabBar { }
    }

    Component {
        id: tabBarStaticTabs
        TabBar {
            TabButton {
                text: "0"
            }
            TabButton {
                text: "1"
            }
        }
    }

    Component {
        id: tabBarStaticTabsCurrent
        TabBar {
            currentIndex: 1
            TabButton {
                text: "0"
            }
            TabButton {
                text: "1"
            }
        }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_defaults() {
        var control = tabBar.createObject(testCase)
        verify(control)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentItem, null)
        control.destroy()
    }

    function test_current() {
        var control = tabBar.createObject(testCase)

        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentItem, null)

        control.addItem(tabButton.createObject(control, {text: "0"}))
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(control.currentItem.checked, true)

        control.addItem(tabButton.createObject(control, {text: "1"}))
        compare(control.count, 2)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(control.currentItem.checked, true)

        control.addItem(tabButton.createObject(control, {text: "2"}))
        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(control.currentItem.checked, true)

        control.currentIndex = 1
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "1")
        compare(control.currentItem.checked, true)

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.currentItem.checked, true)

        control.decrementCurrentIndex()
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "1")
        compare(control.currentItem.checked, true)

        control.incrementCurrentIndex()
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.currentItem.checked, true)

        control.destroy()
    }

    function test_current_static() {
        var control = tabBarStaticTabs.createObject(testCase)

        compare(control.count, 2)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(control.currentItem.checked, true)

        control.destroy()

        control = tabBarStaticTabsCurrent.createObject(testCase)

        compare(control.count, 2)
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "1")
        compare(control.currentItem.checked, true)

        control.destroy()
    }

    function test_addRemove() {
        var control = tabBar.createObject(testCase)

        function verifyCurrentIndexCountDiff() {
            verify(control.currentIndex < control.count)
        }
        control.currentIndexChanged.connect(verifyCurrentIndexCountDiff)
        control.countChanged.connect(verifyCurrentIndexCountDiff)

        var contentChildrenSpy = signalSpy.createObject(testCase, {target: control, signalName: "contentChildrenChanged"})
        verify(contentChildrenSpy.valid)

        compare(control.count, 0)
        compare(control.currentIndex, -1)
        control.addItem(tabButton.createObject(control, {text: "1"}))
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "1")
        compare(contentChildrenSpy.count, 1)

        control.addItem(tabButton.createObject(control, {text: "2"}))
        compare(control.count, 2)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "1")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "2")
        compare(contentChildrenSpy.count, 2)

        control.currentIndex = 1

        control.insertItem(1, tabButton.createObject(control, {text: "3"}))
        compare(control.count, 3)
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "3")
        compare(control.itemAt(2).text, "2")
        compare(contentChildrenSpy.count, 4) // append + insert->move

        control.insertItem(0, tabButton.createObject(control, {text: "4"}))
        compare(control.count, 4)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")
        compare(contentChildrenSpy.count, 6) // append + insert->move

        control.insertItem(control.count, tabButton.createObject(control, {text: "5"}))
        compare(control.count, 5)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")
        compare(control.itemAt(4).text, "5")
        compare(contentChildrenSpy.count, 7)

        control.removeItem(control.count - 1)
        compare(control.count, 4)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")
        compare(contentChildrenSpy.count, 8)

        control.removeItem(0)
        compare(control.count, 3)
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "3")
        compare(control.itemAt(2).text, "2")
        compare(contentChildrenSpy.count, 9)

        control.removeItem(1)
        compare(control.count, 2)
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "2")
        compare(contentChildrenSpy.count, 10)

        control.removeItem(1)
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "1")
        compare(control.itemAt(0).text, "1")
        compare(contentChildrenSpy.count, 11)

        control.removeItem(0)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(contentChildrenSpy.count, 12)

        control.destroy()
    }

    Component {
        id: contentBar
        TabBar {
            QtObject { objectName: "object" }
            TabButton { objectName: "button1" }
            Timer { objectName: "timer" }
            TabButton { objectName: "button2" }
            Component { TabButton { } }
        }
    }

    function test_content() {
        var control = contentBar.createObject(testCase)

        function compareObjectNames(content, names) {
            if (content.length !== names.length)
                return false
            for (var i = 0; i < names.length; ++i) {
                if (content[i].objectName !== names[i])
                    return false
            }
            return true
        }

        var contentChildrenSpy = signalSpy.createObject(testCase, {target: control, signalName: "contentChildrenChanged"})
        verify(contentChildrenSpy.valid)

        verify(compareObjectNames(control.contentData, ["object", "button1", "timer", "button2", ""]))
        verify(compareObjectNames(control.contentChildren, ["button1", "button2"]))

        control.addItem(tabButton.createObject(control, {objectName: "button3"}))
        verify(compareObjectNames(control.contentData, ["object", "button1", "timer", "button2", "", "button3"]))
        verify(compareObjectNames(control.contentChildren, ["button1", "button2", "button3"]))
        compare(contentChildrenSpy.count, 1)

        control.insertItem(0, tabButton.createObject(control, {objectName: "button4"}))
        verify(compareObjectNames(control.contentData, ["object", "button1", "timer", "button2", "", "button3", "button4"]))
        verify(compareObjectNames(control.contentChildren, ["button4", "button1", "button2", "button3"]))
        compare(contentChildrenSpy.count, 3) // append + insert->move

        control.moveItem(1, 2)
        verify(compareObjectNames(control.contentData, ["object", "button1", "timer", "button2", "", "button3", "button4"]))
        verify(compareObjectNames(control.contentChildren, ["button4", "button2", "button1", "button3"]))
        compare(contentChildrenSpy.count, 4)

        control.removeItem(0)
        verify(compareObjectNames(control.contentData, ["object", "button1", "timer", "button2", "", "button3"]))
        verify(compareObjectNames(control.contentChildren, ["button2", "button1", "button3"]))
        compare(contentChildrenSpy.count, 5)

        control.destroy()
    }

    Component {
        id: repeated
        TabBar {
            property alias repeater: repeater
            Repeater {
                id: repeater
                model: 5
                TabButton { property int idx: index }
            }
        }
    }

    function test_repeater() {
        var control = repeated.createObject(testCase)
        verify(control)

        var model = control.contentModel
        verify(model)

        var repeater = control.repeater
        verify(repeater)

        compare(repeater.count, 5)
        compare(model.count, 5)

        for (var i = 0; i < 5; ++i) {
            var item1 = control.itemAt(i)
            verify(item1)
            compare(item1.idx, i)
            compare(model.get(i), item1)
            compare(repeater.itemAt(i), item1)
        }

        repeater.model = 3
        compare(repeater.count, 3)
        compare(model.count, 3)

        for (var j = 0; j < 3; ++j) {
            var item2 = control.itemAt(j)
            verify(item2)
            compare(item2.idx, j)
            compare(model.get(j), item2)
            compare(repeater.itemAt(j), item2)
        }

        control.destroy()
    }

    Component {
        id: ordered
        TabBar {
            id: obar
            property alias repeater: repeater
            TabButton { text: "static_1" }
            Repeater {
                id: repeater
                model: 2
                TabButton { text: "repeated_" + (index + 2) }
            }
            TabButton { text: "static_4" }
            Component.onCompleted: {
                addItem(tabButton.createObject(obar, {text: "dynamic_5"}))
                addItem(tabButton.createObject(obar.contentItem, {text: "dynamic_6"}))
                insertItem(0, tabButton.createObject(obar, {text: "dynamic_0"}))
            }
        }
    }

    function test_order() {
        var control = ordered.createObject(testCase)
        verify(control)

        compare(control.count, 7)
        compare(control.itemAt(0).text, "dynamic_0")
        compare(control.itemAt(1).text, "static_1")
        compare(control.itemAt(2).text, "repeated_2")
        compare(control.itemAt(3).text, "repeated_3")
        compare(control.itemAt(4).text, "static_4")
        compare(control.itemAt(5).text, "dynamic_5")
        compare(control.itemAt(6).text, "dynamic_6")

        control.destroy()
    }

    function test_move_data() {
        return [
            {tag:"0->1 (0)", from: 0, to: 1, currentBefore: 0, currentAfter: 1},
            {tag:"0->1 (1)", from: 0, to: 1, currentBefore: 1, currentAfter: 0},
            {tag:"0->1 (2)", from: 0, to: 1, currentBefore: 2, currentAfter: 2},

            {tag:"0->2 (0)", from: 0, to: 2, currentBefore: 0, currentAfter: 2},
            {tag:"0->2 (1)", from: 0, to: 2, currentBefore: 1, currentAfter: 0},
            {tag:"0->2 (2)", from: 0, to: 2, currentBefore: 2, currentAfter: 1},

            {tag:"1->0 (0)", from: 1, to: 0, currentBefore: 0, currentAfter: 1},
            {tag:"1->0 (1)", from: 1, to: 0, currentBefore: 1, currentAfter: 0},
            {tag:"1->0 (2)", from: 1, to: 0, currentBefore: 2, currentAfter: 2},

            {tag:"1->2 (0)", from: 1, to: 2, currentBefore: 0, currentAfter: 0},
            {tag:"1->2 (1)", from: 1, to: 2, currentBefore: 1, currentAfter: 2},
            {tag:"1->2 (2)", from: 1, to: 2, currentBefore: 2, currentAfter: 1},

            {tag:"2->0 (0)", from: 2, to: 0, currentBefore: 0, currentAfter: 1},
            {tag:"2->0 (1)", from: 2, to: 0, currentBefore: 1, currentAfter: 2},
            {tag:"2->0 (2)", from: 2, to: 0, currentBefore: 2, currentAfter: 0},

            {tag:"2->1 (0)", from: 2, to: 1, currentBefore: 0, currentAfter: 0},
            {tag:"2->1 (1)", from: 2, to: 1, currentBefore: 1, currentAfter: 2},
            {tag:"2->1 (2)", from: 2, to: 1, currentBefore: 2, currentAfter: 1},

            {tag:"0->0", from: 0, to: 0, currentBefore: 0, currentAfter: 0},
            {tag:"-1->0", from: 0, to: 0, currentBefore: 1, currentAfter: 1},
            {tag:"0->-1", from: 0, to: 0, currentBefore: 2, currentAfter: 2},
            {tag:"1->10", from: 0, to: 0, currentBefore: 0, currentAfter: 0},
            {tag:"10->2", from: 0, to: 0, currentBefore: 1, currentAfter: 1},
            {tag:"10->-1", from: 0, to: 0, currentBefore: 2, currentAfter: 2}
        ]
    }

    function test_move(data) {
        var control = tabBar.createObject(testCase)

        compare(control.count, 0)
        var titles = ["1", "2", "3"]

        var i = 0;
        for (i = 0; i < titles.length; ++i)
            control.addItem(tabButton.createObject(control, {text: titles[i]}))

        compare(control.count, titles.length)
        for (i = 0; i < control.count; ++i)
            compare(control.itemAt(i).text, titles[i])

        control.currentIndex = data.currentBefore
        control.moveItem(data.from, data.to)

        compare(control.count, titles.length)
        compare(control.currentIndex, data.currentAfter)

        var title = titles[data.from]
        titles.splice(data.from, 1)
        titles.splice(data.to, 0, title)

        compare(control.count, titles.length)
        for (i = 0; i < control.count; ++i)
            compare(control.itemAt(i).text, titles[i])

        control.destroy()
    }

    Component {
        id: dynamicBar
        TabBar {
            id: dbar
            TabButton { text: "static" }
            Component.onCompleted: {
                addItem(tabButton.createObject(dbar, {text: "added"}))
                insertItem(0, tabButton.createObject(dbar, {text: "inserted"}))
                tabButton.createObject(dbar, {text: "dynamic"})
            }
        }
    }

    function test_dynamic() {
        var control = dynamicBar.createObject(testCase)

        // insertItem(), addItem(), createObject() and static TabButton {}
        compare(control.count, 4)
        compare(control.itemAt(0).text, "inserted")

        var tab = tabButton.createObject(control, {text: "dying"})
        compare(control.count, 5)
        compare(control.itemAt(4).text, "dying")

        // TODO: fix crash in QQuickItemView
//        tab.destroy()
//        wait(0)
//        compare(control.count, 4)

        control.destroy()
    }

    function test_layout_data() {
        return [
            { tag: "spacing:0", spacing: 0 },
            { tag: "spacing:1", spacing: 1 },
            { tag: "spacing:10", spacing: 10 },
        ]
    }

    function test_layout(data) {
        var control = tabBar.createObject(testCase, {spacing: data.spacing, width: 200})

        var tab1 = tabButton.createObject(control, {text: "First"})
        control.addItem(tab1)
        tryCompare(tab1, "width", control.width)

        var tab2 = tabButton.createObject(control, {text: "Second"})
        control.addItem(tab2)
        tryCompare(tab1, "width", (control.width - data.spacing) / 2)
        compare(tab2.width, (control.width - data.spacing) / 2)

        var tab3 = tabButton.createObject(control, {width: 50, text: "Third"})
        control.addItem(tab3)
        tryCompare(tab1, "width", (control.width - 2 * data.spacing - 50) / 2)
        compare(tab2.width, (control.width - 2 * data.spacing - 50) / 2)
        compare(tab3.width, 50)

        var expectedWidth = tab3.contentItem.implicitWidth + tab3.leftPadding + tab3.rightPadding
        tab3.width = tab3.implicitWidth
        tryCompare(tab1, "width", (control.width - 2 * data.spacing - expectedWidth) / 2)
        tryCompare(tab2, "width", (control.width - 2 * data.spacing - expectedWidth) / 2)
        compare(tab3.width, expectedWidth)

        control.destroy()
    }
}
