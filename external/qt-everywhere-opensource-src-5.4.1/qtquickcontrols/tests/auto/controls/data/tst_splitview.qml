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
import QtQuick.Layouts 1.0
import QtQuickControlsTests 1.0

TestCase {
    id: testCase
    name: "Tests_SplitView"
    when: windowShown
    width: 400
    height: 500
    visible: true
    property int handleWidth: 1
    property int handleHeight: 5

    Component {
        id: splitView
        SplitView {
            anchors.fill: parent
            property alias item1: item1
            property alias item2: item2
            handleDelegate: Rectangle { width: handleWidth; height: handleHeight; color: "black" }

            Rectangle {
                id: item1
                width: 100
                height: 80
                color: "red"
            }
            Rectangle {
                id: item2
                width: 200
                height: 90
                color: "blue"
            }
        }
    }

    Component {
        id: splitView_hide_item_after_fillWidth
        SplitView {
            anchors.fill: parent
            property alias item3: item3
            handleDelegate: Rectangle { width: handleWidth; height: handleHeight; color: "black" }
            Rectangle {
                color: "yellow"
                Layout.fillWidth: true
            }
            Rectangle {
                color: "green"
                Layout.minimumWidth: 100
                visible: false
            }
            Rectangle {
                id: item3
                color: "blue"
                Layout.minimumWidth: 100
            }
        }
    }

    function test_01_splitView() {
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        verify (view.orientation === Qt.Horizontal)
        compare (view.__items.length, 2)
        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)
        compare (view.item2.x, view.item1.x + view.item1.width + handleWidth)
        compare (view.item2.y, 0)
        compare (view.item2.width, testCase.width - view.item1.width - handleWidth)
        compare (view.item2.height, 500)
        view.destroy()
    }

    function test_02_splitView_initial_orientation_vertical() {
        var view = splitView.createObject(testCase, {orientation:Qt.Vertical});
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        compare (view.orientation, Qt.Vertical)
        compare (view.__items.length, 2)
        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 400)
        compare (view.item1.height, 80)
        compare (view.item2.x, 0)
        compare (view.item2.y, view.item1.y + view.item1.height + handleHeight)
        compare (view.item2.width, 400)
        compare (view.item2.height, testCase.height - view.item1.height - handleHeight)
        view.destroy()
    }

    function test_03_orientation_change() {
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        verify (view.orientation === Qt.Horizontal)

        view.orientation = Qt.Vertical
        verify (view.orientation === Qt.Vertical)
        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 400)
        compare (view.item1.height, 100)
        compare (view.item2.x, 0)
        // We use handleWidth rather than handleHeight, since the layout is just flipped:
        compare (view.item2.y, view.item1.y + view.item1.height + handleWidth)
        compare (view.item2.width, 400)
        compare (view.item2.height, testCase.height - view.item1.height - handleWidth)

        view.orientation = Qt.Horizontal
        verify (view.orientation === Qt.Horizontal)
        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)
        compare (view.item2.x, view.item1.x + view.item1.width + handleWidth)
        compare (view.item2.y, 0)
        compare (view.item2.width, testCase.width - view.item1.width - handleWidth)
        compare (view.item2.height, 500)
        view.destroy()
    }

    function test_04_hide_item() {
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        verify (view.item1.visible)
        verify (view.item2.visible)
        view.item1.visible = false
        verify (view.item1.visible === false)

        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)
        compare (view.item2.x, 0)
        compare (view.item2.y, 0)
        compare (view.item2.width, testCase.width)
        compare (view.item2.height, 500)
        view.destroy()
    }

    function test_05_hide_fillWidth_item() {
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        verify (view.item1.visible)
        verify (view.item2.visible)
        view.item2.visible = false
        verify (view.item2.visible === false)

        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)
        compare (view.item2.x, view.item1.x + view.item1.width + handleWidth)
        compare (view.item2.y, 0)
        compare (view.item2.width, testCase.width - view.item1.width - handleWidth)
        compare (view.item2.height, 500)
        view.destroy()
    }

    function test_hide_item_after_fillWidth() {
        // QTBUG-33448
        var view = splitView_hide_item_after_fillWidth.createObject(testCase);
        verify (view !== null, "splitview created is null")
        waitForRendering(view)
        compare (view.item3.x, view.width - view.item3.width)
        view.destroy()
    }

    Component {
        id: item_to_add_dynamically
        Rectangle {
            width: 50
            height: 100
            color: "yellow"
        }
    }

    function test_dynamic_item_add() {
        // QTBUG-35281
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        verify (view.orientation === Qt.Horizontal)
        waitForRendering(view)

        var item3 = item_to_add_dynamically.createObject()
        view.addItem(item3)
        // reset item2 width
        view.item2.width = 200
        waitForRendering(view)

        compare (view.__items.length, 3)

        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)

        compare (view.item2.x, view.item1.x + view.item1.width + handleWidth)
        compare (view.item2.y, 0)
        compare (view.item2.width, 200)
        compare (view.item2.height, 500)

        compare (item3.x, view.item2.x + view.item2.width + handleWidth)
        compare (item3.y, 0)
        compare (item3.width, testCase.width - view.item2.width - view.item1.width - (handleWidth*2))
        compare (item3.height, 500)

        view.destroy()
    }

    function test_dynamic_item_add_fillWidth() {
        var view = splitView.createObject(testCase);
        verify (view !== null, "splitview created is null")
        verify (view.orientation === Qt.Horizontal)
        view.item2.Layout.fillWidth = true
        waitForRendering(view)

        var item3 = item_to_add_dynamically.createObject()
        view.addItem(item3)
        waitForRendering(view)

        compare (view.__items.length, 3)

        compare (view.item1.x, 0)
        compare (view.item1.y, 0)
        compare (view.item1.width, 100)
        compare (view.item1.height, 500)

        compare (view.item2.x, view.item1.x + view.item1.width + handleWidth)
        compare (view.item2.y, 0)
        compare (view.item2.width, testCase.width - view.item1.width - item3.width - (handleWidth*2))
        compare (view.item2.height, 500)

        compare (item3.x, view.item2.x + view.item2.width + handleWidth)
        compare (item3.y, 0)
        compare (item3.width, 50)
        compare (item3.height, 500)

        view.destroy()
    }
}
