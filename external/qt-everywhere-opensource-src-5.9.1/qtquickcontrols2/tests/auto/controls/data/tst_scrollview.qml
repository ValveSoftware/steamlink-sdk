/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
import QtQuick.Controls 2.2

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "ScrollView"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: scrollView
        ScrollView { }
    }

    Component {
        id: scrollableLabel
        ScrollView {
            Label {
                text: "ABC"
                font.pixelSize: 512
            }
        }
    }

    Component {
        id: scrollableLabels
        ScrollView {
            contentHeight: label1.implicitHeight + label2.implicitHeight + label3.implicitHeight
            Label {
                id: label1
                text: "First"
                font.pixelSize: 96
            }
            Label {
                id: label2
                text: "Second"
                font.pixelSize: 96
            }
            Label {
                id: label3
                text: "Third"
                font.pixelSize: 96
            }
        }
    }

    Component {
        id: flickableLabel
        ScrollView {
            Flickable {
                contentWidth: label.implicitWidth
                contentHeight: label.implicitHeight
                Label {
                    id: label
                    text: "ABC"
                    font.pixelSize: 512
                }
            }
        }
    }

    Component {
        id: scrollableListView
        ScrollView {
            ListView {
                model: 3
                delegate: Label {
                    text: modelData
                }
            }
        }
    }

    function test_scrollBars() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200})
        verify(control)

        var vertical = control.ScrollBar.vertical
        verify(vertical)

        var horizontal = control.ScrollBar.horizontal
        verify(horizontal)

        control.contentHeight = 400
        verify(vertical.size > 0)
        compare(control.contentItem.visibleArea.heightRatio, vertical.size)

        control.contentWidth = 400
        verify(horizontal.size > 0)
        compare(control.contentItem.visibleArea.widthRatio, horizontal.size)

        vertical.increase()
        verify(vertical.position > 0)
        compare(control.contentItem.visibleArea.yPosition, vertical.position)

        horizontal.increase()
        verify(horizontal.position > 0)
        compare(control.contentItem.visibleArea.xPosition, horizontal.position)
    }

    function test_oneChild_data() {
        return [
            { tag: "label", component: scrollableLabel },
            { tag: "flickable", component: flickableLabel }
        ]
    }

    function test_oneChild(data) {
        var control = createTemporaryObject(data.component, testCase)
        verify(control)

        var flickable = control.contentItem
        verify(flickable.hasOwnProperty("contentX"))
        verify(flickable.hasOwnProperty("contentY"))

        var label = flickable.contentItem.children[0]
        compare(label.text, "ABC")

        compare(control.implicitWidth, label.implicitWidth)
        compare(control.implicitHeight, label.implicitHeight)

        compare(control.contentWidth, label.implicitWidth)
        compare(control.contentHeight, label.implicitHeight)

        compare(flickable.contentWidth, label.implicitWidth)
        compare(flickable.contentHeight, label.implicitHeight)

        control.contentWidth = 200
        compare(control.implicitWidth, 200)
        compare(control.contentWidth, 200)
        compare(flickable.contentWidth, 200)

        control.contentHeight = 100
        compare(control.implicitHeight, 100)
        compare(control.contentHeight, 100)
        compare(flickable.contentHeight, 100)
    }

    function test_multipleChildren() {
        var control = createTemporaryObject(scrollableLabels, testCase)
        verify(control)

        var flickable = control.contentItem
        verify(flickable.hasOwnProperty("contentX"))
        verify(flickable.hasOwnProperty("contentY"))

        compare(control.contentChildren, flickable.contentItem.children)

        var label1 = control.contentChildren[0]
        compare(label1.text, "First")

        var label2 = control.contentChildren[1]
        compare(label2.text, "Second")

        var label3 = control.contentChildren[2]
        compare(label3.text, "Third")

        var expectedContentHeight = label1.implicitHeight + label2.implicitHeight + label3.implicitHeight
        compare(control.contentHeight, expectedContentHeight)
        compare(flickable.contentHeight, expectedContentHeight)
    }

    function test_listView() {
        var control = createTemporaryObject(scrollableListView, testCase)
        verify(control)

        var listview = control.contentItem
        verify(listview.hasOwnProperty("contentX"))
        verify(listview.hasOwnProperty("contentY"))
        verify(listview.hasOwnProperty("model"))

        compare(control.contentWidth, listview.contentWidth)
        compare(control.contentHeight, listview.contentHeight)
    }

    function test_mouse() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200, contentHeight: 400})
        verify(control)

        mousePress(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.contentItem.contentY, 0)

        for (var y = control.height / 2; y >= 0; --y) {
            mouseMove(control, control.width / 2, y, 10)
            compare(control.contentItem.contentY, 0)
        }

        mouseRelease(control, control.width / 2, 0, Qt.LeftButton)
        compare(control.contentItem.contentY, 0)
    }

    function test_hover() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200, contentHeight: 400})
        verify(control)

        var vertical = control.ScrollBar.vertical
        verify(vertical)
        vertical.hoverEnabled = true

        mouseMove(vertical, vertical.width / 2, vertical.height / 2)
        compare(vertical.visible, true)
        compare(vertical.hovered, true)
        compare(vertical.active, true)
        compare(vertical.interactive, true)
    }

    function test_wheel() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200, contentHeight: 400})
        verify(control)

        var vertical = control.ScrollBar.vertical
        verify(vertical)

        mouseWheel(control, control.width / 2, control.height / 2, 0, -120)
        compare(vertical.visible, true)
        compare(vertical.active, true)
        compare(vertical.interactive, true)
    }

    function test_touch() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200, contentHeight: 400})
        verify(control)

        var vertical = control.ScrollBar.vertical
        verify(vertical)

        var touch = touchEvent(control)
        touch.press(0, control, control.width / 2, control.height / 2).commit()
        compare(control.contentItem.contentY, 0)

        compare(vertical.active, false)
        compare(vertical.interactive, false)

        var maxContentY = 0
        for (var y = control.height / 2; y >= 0; --y) {
            touch.move(0, control, control.width / 2, y).commit()
            maxContentY = Math.max(maxContentY, control.contentItem.contentY)
        }
        verify(maxContentY > 0)

        compare(vertical.active, true)
        compare(vertical.interactive, false)

        touch.release(0, control, control.width / 2, 0).commit()
    }

    function test_keys() {
        var control = createTemporaryObject(scrollView, testCase, {width: 200, height: 200, contentWidth: 400, contentHeight: 400})
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        var vertical = control.ScrollBar.vertical
        verify(vertical)

        compare(vertical.position, 0.0)
        for (var i = 1; i <= 10; ++i) {
            keyClick(Qt.Key_Down)
            compare(vertical.position, Math.min(0.5, i * 0.1))
        }
        compare(vertical.position, 0.5)
        for (i = 1; i <= 10; ++i) {
            keyClick(Qt.Key_Up)
            compare(vertical.position, Math.max(0.0, 0.5 - i * 0.1))
        }
        compare(vertical.position, 0.0)

        var horizontal = control.ScrollBar.horizontal
        verify(horizontal)

        compare(horizontal.position, 0.0)
        for (i = 1; i <= 10; ++i) {
            keyClick(Qt.Key_Right)
            compare(horizontal.position, Math.min(0.5, i * 0.1))
        }
        compare(horizontal.position, 0.5)
        for (i = 1; i <= 10; ++i) {
            keyClick(Qt.Key_Left)
            compare(horizontal.position, Math.max(0.0, 0.5 - i * 0.1))
        }
        compare(horizontal.position, 0.0)
    }
}
