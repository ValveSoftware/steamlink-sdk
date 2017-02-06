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
import QtQuick.Controls.Styles 1.1
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 400
    height: 400

TestCase {
    id: testCase
    name: "Tests_ScrollView"
    when:windowShown
    width:400
    height:400


    TextArea { id: textArea }

    Item { id: bigItem  }

    Component {
        id: scrollViewComponent
        ScrollView { }
    }

    function test_scroll() {
        var component = scrollViewComponent
        var scrollView = component.createObject(testCase);
        verify(scrollView !== null, "table created is null")

        scrollView.contentItem = bigItem
        scrollView.visible = true
        verify(scrollView.flickableItem !== null, "flickableItem should not be null")
        verify(scrollView.flickableItem !== scrollView.contentItem)
        verify(scrollView.flickableItem.contentHeight === 0, "ContentHeight not set")

        bigItem.height = 222
        bigItem.width = 333

        verify(scrollView.flickableItem.contentHeight === 222, "ContentHeight not set")
        verify(scrollView.flickableItem.contentWidth === 333, "ContentHeight not set")

        scrollView.flickableItem.contentY = 200
        verify(scrollView.flickableItem.contentY === 200, "ContentY not set")

        scrollView.flickableItem.contentX = 300
        verify(scrollView.flickableItem.contentX === 300, "ContentX not set")
        scrollView.destroy()
    }

    Component {
        id: dragFetchAppendComponent

        ScrollView {
            width: 400; height: 400
            frameVisible: false
            style: ScrollViewStyle {
                transientScrollBars: false
                handle: Rectangle {
                    implicitWidth: 16; implicitHeight: 16
                    color: "red"
                }
                scrollBarBackground: Item {width: 16 ; height: 16}
                incrementControl: Rectangle {
                    width: 16; height: 16
                    color: "blue"
                }
                decrementControl: Rectangle {
                    width: 16; height: 16
                    color: "blue"
                }
            }
            ListView {
                id: view

                verticalLayoutDirection: ListView.BottomToTop
                model: TestFetchAppendModel { }
                delegate: Text {
                    width: view.width
                    height: 60
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "Item %1".arg(model.index)
                }
            }
        }
    }

    function test_dragFetchAppend() {   // QTBUG-50795
        var scrollView = dragFetchAppendComponent.createObject(container)
        verify(scrollView !== null, "view created is null")
        waitForRendering(scrollView)
        tryCompare(scrollView.flickableItem, "contentHeight", 60 * 20)

        // After scrolling to the end, view should ask the model to fetch more
        // data, content height should increase and scrollbar handle should move
        // to the center.
        mouseDrag(scrollView, scrollView.width - 2, scrollView.height - 8 - 16, 0, -scrollView.height + 8 + 16)
        waitForRendering(scrollView)

        // Move it again to fetch more data from the model.
        mouseDrag(scrollView, scrollView.width - 2, scrollView.height / 2, 0, -scrollView.height / 2  + 8 + 16)
        waitForRendering(scrollView)

        mouseRelease(scrollView, scrollView.width - 2, 8 + 16)
        waitForRendering(scrollView)

        verify(Math.round(scrollView.flickableItem.contentHeight) > 60 * 20)
        verify(Math.round(scrollView.flickableItem.contentY) < -(60 * 20))

        scrollView.destroy()
    }

    function test_scrollbars() {
        var component = scrollViewComponent
        var scrollView = component.createObject(testCase);
        scrollView.contentItem = bigItem
        scrollView.parent = container

        bigItem.height = 100
        bigItem.width = 100

        verify(!scrollView.__horizontalScrollBar.visible, "Scrollbar showing when contents already fit")
        verify(!scrollView.__verticalScrollBar.visible, "Scrollbar showing when contents already fit")

        bigItem.height = 1000
        bigItem.width = 1000

        verify(scrollView.__horizontalScrollBar.visible, "Scrollbar not showing when contents are too big")
        verify(scrollView.__verticalScrollBar.visible, "Scrollbar not showing when contents are too big")

        //always off
        bigItem.height = 1000
        scrollView.verticalScrollBarPolicy = Qt.ScrollBarAlwaysOff
        verify(!scrollView.__verticalScrollBar.visible, "Scrollbar showing when disabled")
        bigItem.height = 100
        verify(!scrollView.__verticalScrollBar.visible, "Scrollbar showing when disabled")

        //always on
        scrollView.verticalScrollBarPolicy = Qt.ScrollBarAlwaysOn
        bigItem.height = 1000
        verify(scrollView.__verticalScrollBar.visible, "Scrollbar not showing when forced on")
        bigItem.height = 100
        verify(scrollView.__verticalScrollBar.visible, "Scrollbar not showing when forced on")

        scrollView.destroy()
    }

    function test_clickToCenter() {

        var test_control = 'import QtQuick 2.2;                       \
        import QtQuick.Controls 1.2;                                  \
        import QtQuick.Controls.Styles 1.1;                           \
        ScrollView {                                                  \
            id: _control1;                                            \
            width: 100 ; height: 100;                                 \
            Item { width: 200; height: 200 }                          \
            activeFocusOnTab: true;                                   \
            style:ScrollViewStyle{                                    \
                   handle: Item {width: 16 ; height: 16}              \
                   scrollBarBackground: Item {width: 16 ; height: 16} \
                   incrementControl: Item {width: 16 ; height: 16}    \
                   decrementControl: Item {width: 16 ; height: 16}}   }'

        var scrollView = Qt.createQmlObject(test_control, container, '')
        verify(scrollView !== null, "view created is null")
        verify(scrollView.flickableItem.contentY === 0)

        mouseClick(scrollView, scrollView.width -2, scrollView.height/2, Qt.LeftButton)
        verify(Math.round(scrollView.flickableItem.contentY) === 100)

        verify(scrollView.flickableItem.contentX === 0)
        mouseClick(scrollView, scrollView.width/2, scrollView.height - 2, Qt.LeftButton)
        verify(Math.round(scrollView.flickableItem.contentX) === 100)
    }

    function test_viewport() {
        var component = scrollViewComponent
        var scrollView =  component.createObject(testCase);
        verify(scrollView !== null, "table created is null")

        scrollView.forceActiveFocus();
        verify(scrollView.viewport, "Viewport not defined")
        verify(!scrollView.contentItem, "contentItem should be null")
        verify(!scrollView.flickableItem, "flickableItem should be null")
        verify(!scrollView.frameVisible, "Frame should be false")

        scrollView.contentItem = textArea
        verify(scrollView.viewport, "Viewport should be defined")
        verify(scrollView.contentItem, "contentItem should not be null")
        verify(scrollView.flickableItem, "flickableItem should not be null")
        verify(scrollView.flickableItem.contentHeight === textArea.height, "Content height not set")

        var prevViewportWidth  = scrollView.viewport.width
        scrollView.frameVisible = true
        verify(scrollView.frameVisible, "Frame should be true")
        verify(scrollView.viewport.width < prevViewportWidth, "Viewport should be smaller with frame")
        scrollView.destroy()
    }

    function test_activeFocusOnTab() {
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
        ScrollView {                        \
            id: _control1;                  \
            activeFocusOnTab: true;         \
        }                                   \
        ScrollView {                        \
            id: _control2;                  \
            activeFocusOnTab: false;        \
        }                                   \
        ScrollView {                        \
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
