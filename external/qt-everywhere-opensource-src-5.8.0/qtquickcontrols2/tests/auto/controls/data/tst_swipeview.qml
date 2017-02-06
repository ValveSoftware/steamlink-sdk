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
    name: "SwipeView"

    Component {
        id: swipeView
        SwipeView { }
    }

    Component {
        id: page
        Text { }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_current() {
        var control = swipeView.createObject(testCase)

        var currentItemChangedSpy = signalSpy.createObject(testCase, {target: control, signalName: "currentItemChanged"})
        verify(currentItemChangedSpy.valid)

        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentItem, null)

        control.addItem(page.createObject(control, {text: "0"}))
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(currentItemChangedSpy.count, 1);

        control.addItem(page.createObject(control, {text: "1"}))
        compare(control.count, 2)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(currentItemChangedSpy.count, 1);

        control.addItem(page.createObject(control, {text: "2"}))
        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentItem.text, "0")
        compare(currentItemChangedSpy.count, 1);

        control.currentIndex = 1
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "1")
        compare(currentItemChangedSpy.count, 2);

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(currentItemChangedSpy.count, 3);

        control.decrementCurrentIndex()
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "1")
        compare(currentItemChangedSpy.count, 4);

        control.incrementCurrentIndex()
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(currentItemChangedSpy.count, 5);

        control.destroy()
    }

    Component {
        id: initialCurrentSwipeView
        SwipeView {
            currentIndex: 1

            property alias item0: item0
            property alias item1: item1

            Item {
                id: item0
            }
            Item {
                id: item1
            }
        }
    }

    function test_initialCurrent() {
        var control = initialCurrentSwipeView.createObject(testCase)

        compare(control.count, 2)
        compare(control.currentIndex, 1)
        compare(control.currentItem, control.item1)

        control.destroy()
    }

    function test_addRemove() {
        var control = swipeView.createObject(testCase)

        function verifyCurrentIndexCountDiff() {
            verify(control.currentIndex < control.count)
        }
        control.currentIndexChanged.connect(verifyCurrentIndexCountDiff)
        control.countChanged.connect(verifyCurrentIndexCountDiff)

        var currentItemChangedSpy = signalSpy.createObject(testCase, {target: control, signalName: "currentItemChanged"})
        verify(currentItemChangedSpy.valid)

        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentItem, null)
        control.addItem(page.createObject(control, {text: "1"}))
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(currentItemChangedSpy.count, 1)
        compare(control.currentItem.text, "1")
        control.addItem(page.createObject(control, {text: "2"}))
        compare(control.count, 2)
        compare(control.currentIndex, 0)
        compare(currentItemChangedSpy.count, 1)
        compare(control.currentItem.text, "1")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "2")

        control.currentIndex = 1
        compare(currentItemChangedSpy.count, 2)

        control.insertItem(1, page.createObject(control, {text: "3"}))
        compare(control.count, 3)
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "3")
        compare(control.itemAt(2).text, "2")

        control.insertItem(0, page.createObject(control, {text: "4"}))
        compare(control.count, 4)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")

        control.insertItem(control.count, page.createObject(control, {text: "5"}))
        compare(control.count, 5)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")
        compare(control.itemAt(4).text, "5")

        control.removeItem(control.count - 1)
        compare(control.count, 4)
        compare(control.currentIndex, 3)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "4")
        compare(control.itemAt(1).text, "1")
        compare(control.itemAt(2).text, "3")
        compare(control.itemAt(3).text, "2")

        control.removeItem(0)
        compare(control.count, 3)
        compare(control.currentIndex, 2)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "3")
        compare(control.itemAt(2).text, "2")

        control.removeItem(1)
        compare(control.count, 2)
        compare(control.currentIndex, 1)
        compare(control.currentItem.text, "2")
        compare(control.itemAt(0).text, "1")
        compare(control.itemAt(1).text, "2")

        currentItemChangedSpy.clear()

        control.removeItem(1)
        compare(control.count, 1)
        compare(control.currentIndex, 0)
        compare(currentItemChangedSpy.count, 1)
        compare(control.currentItem.text, "1")
        compare(control.itemAt(0).text, "1")

        control.removeItem(0)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(currentItemChangedSpy.count, 2)

        control.destroy()
    }

    Component {
        id: contentView
        SwipeView {
            QtObject { objectName: "object" }
            Item { objectName: "page1" }
            Timer { objectName: "timer" }
            Item { objectName: "page2" }
            Component { Item { } }
        }
    }

    function test_content() {
        var control = contentView.createObject(testCase)

        function compareObjectNames(content, names) {
            if (content.length !== names.length)
                return false
            for (var i = 0; i < names.length; ++i) {
                if (content[i].objectName !== names[i])
                    return false
            }
            return true
        }

        verify(compareObjectNames(control.contentData, ["object", "page1", "timer", "page2", ""]))
        verify(compareObjectNames(control.contentChildren, ["page1", "page2"]))

        control.addItem(page.createObject(control, {objectName: "page3"}))
        verify(compareObjectNames(control.contentData, ["object", "page1", "timer", "page2", "", "page3"]))
        verify(compareObjectNames(control.contentChildren, ["page1", "page2", "page3"]))

        control.insertItem(0, page.createObject(control, {objectName: "page4"}))
        verify(compareObjectNames(control.contentData, ["object", "page1", "timer", "page2", "", "page3", "page4"]))
        verify(compareObjectNames(control.contentChildren, ["page4", "page1", "page2", "page3"]))

        control.moveItem(1, 2)
        verify(compareObjectNames(control.contentData, ["object", "page1", "timer", "page2", "", "page3", "page4"]))
        verify(compareObjectNames(control.contentChildren, ["page4", "page2", "page1", "page3"]))

        control.removeItem(0)
        verify(compareObjectNames(control.contentData, ["object", "page1", "timer", "page2", "", "page3"]))
        verify(compareObjectNames(control.contentChildren, ["page2", "page1", "page3"]))

        control.destroy()
    }

    Component {
        id: repeated
        SwipeView {
            property alias repeater: repeater
            Repeater {
                id: repeater
                model: 5
                Item { property int idx: index }
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
        SwipeView {
            id: oview
            property alias repeater: repeater
            Text { text: "static_1" }
            Repeater {
                id: repeater
                model: 2
                Text { text: "repeated_" + (index + 2) }
            }
            Text { text: "static_4" }
            Component.onCompleted: {
                addItem(page.createObject(oview, {text: "dynamic_5"}))
                addItem(page.createObject(oview.contentItem, {text: "dynamic_6"}))
                insertItem(0, page.createObject(oview, {text: "dynamic_0"}))
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

    Component {
        id: pageAttached

        Text {
            property int index: SwipeView.index
            property SwipeView view: SwipeView.view
            property bool isCurrentItem: SwipeView.isCurrentItem
            property bool isNextItem: SwipeView.isNextItem
            property bool isPreviousItem: SwipeView.isPreviousItem
        }
    }

    function test_move(data) {
        var control = swipeView.createObject(testCase)

        compare(control.count, 0)
        var titles = ["1", "2", "3"]

        var i = 0;
        for (i = 0; i < titles.length; ++i) {
            var item = pageAttached.createObject(control, {text: titles[i]})
            control.addItem(item)
        }

        compare(control.count, titles.length)
        for (i = 0; i < control.count; ++i) {
            compare(control.itemAt(i).text, titles[i])
            compare(control.itemAt(i).SwipeView.index, i)
            compare(control.itemAt(i).SwipeView.isCurrentItem, i === 0)
            compare(control.itemAt(i).SwipeView.isNextItem, i === 1)
            compare(control.itemAt(i).SwipeView.isPreviousItem, false)
        }

        control.currentIndex = data.currentBefore
        for (i = 0; i < control.count; ++i) {
            compare(control.itemAt(i).SwipeView.isCurrentItem, i === data.currentBefore)
            compare(control.itemAt(i).SwipeView.isNextItem, i === data.currentBefore + 1)
            compare(control.itemAt(i).SwipeView.isPreviousItem, i === data.currentBefore - 1)
        }

        control.moveItem(data.from, data.to)

        compare(control.count, titles.length)
        compare(control.currentIndex, data.currentAfter)

        var title = titles[data.from]
        titles.splice(data.from, 1)
        titles.splice(data.to, 0, title)

        compare(control.count, titles.length)
        for (i = 0; i < control.count; ++i) {
            compare(control.itemAt(i).text, titles[i])
            compare(control.itemAt(i).SwipeView.index, i);
            compare(control.itemAt(i).SwipeView.isCurrentItem, i === data.currentAfter)
            compare(control.itemAt(i).SwipeView.isNextItem, i === data.currentAfter + 1)
            compare(control.itemAt(i).SwipeView.isPreviousItem, i === data.currentAfter - 1)
        }

        control.destroy()
    }

    Component {
        id: dynamicView
        SwipeView {
            id: dview
            Text { text: "static" }
            Component.onCompleted: {
                addItem(page.createObject(dview, {text: "added"}))
                insertItem(0, page.createObject(dview, {text: "inserted"}))
                page.createObject(dview, {text: "dynamic"})
            }
        }
    }

    function test_dynamic() {
        var control = dynamicView.createObject(testCase)

        // insertItem(), addItem(), createObject() and static page {}
        compare(control.count, 4)
        compare(control.itemAt(0).text, "inserted")

        var tab = page.createObject(control, {text: "dying"})
        compare(control.count, 5)
        compare(control.itemAt(4).text, "dying")

        // TODO: fix crash in QQuickItemView
//        tab.destroy()
//        wait(0)
//        compare(control.count, 4)

        control.destroy()
    }

    function test_attachedParent() {
        var control = swipeView.createObject(testCase);

        var page = pageAttached.createObject(testCase);
        compare(page.view, null);
        compare(page.index, -1);
        compare(page.isCurrentItem, false);
        compare(page.isNextItem, false);
        compare(page.isPreviousItem, false);
        page.destroy();

        page = pageAttached.createObject(null);
        compare(page.view, null);
        compare(page.index, -1);
        compare(page.isCurrentItem, false);
        compare(page.isNextItem, false);
        compare(page.isPreviousItem, false);

        control.insertItem(0, page);
        compare(control.count, 1);
        compare(page.parent, control.contentItem.contentItem);
        compare(page.view, control);
        compare(page.index, 0);
        compare(page.isCurrentItem, true);
        compare(page.isNextItem, false);
        compare(page.isPreviousItem, false);

        control.removeItem(0);
        compare(control.count, 0);
        compare(page.parent, null);
        compare(page.view, null);
        compare(page.index, -1);
        compare(page.isCurrentItem, false);
        compare(page.isNextItem, false);
        compare(page.isPreviousItem, false);

        control.destroy();
    }
}
