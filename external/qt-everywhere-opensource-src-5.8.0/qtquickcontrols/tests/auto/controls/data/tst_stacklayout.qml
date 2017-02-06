/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Layouts 1.3

Item {
    id: container
    width: 200
    height: 200
    TestCase {
        id: testCase
        name: "Tests_StackLayout"
        when: windowShown
        width: 200
        height: 200

        function geometry(item) {
            return [item.x, item.y, item.width, item.height]
        }

        Component {
            id: countGeometryChanges_Component
            StackLayout {
                id: stack
                property alias col: _col
                property alias row: _row
                width: 100
                ColumnLayout {
                    id: _col
                    property alias r1: _r1
                    property alias r2: _r2
                    property alias r3: _r3
                    spacing: 0
                    property int counter : 0
                    onWidthChanged: { ++counter; }
                    Rectangle {
                        id: _r1
                        implicitWidth: 20
                        implicitHeight: 20
                        Layout.fillWidth: true
                        property int counter : 0
                        onWidthChanged: { ++counter; }
                    }
                    Rectangle {
                        id: _r2
                        implicitWidth: 50
                        implicitHeight: 50
                        Layout.fillWidth: true
                        property int counter : 0
                        onWidthChanged: { ++counter; }
                    }
                    Rectangle {
                        id: _r3
                        implicitWidth: 40
                        implicitHeight: 40
                        Layout.fillWidth: true
                        property int counter : 0
                        onWidthChanged: { ++counter; }
                    }
                }
                RowLayout {
                    id: _row
                    property alias r5: _r5
                    spacing: 0
                    property int counter : 0
                    onWidthChanged: { ++counter; }
                    Rectangle {
                        id: _r5
                        implicitWidth: 100
                        implicitHeight: 100
                        Layout.fillWidth: true
                        property int counter : 0
                        onWidthChanged: { ++counter; }
                    }
                }
            }
        }

        function test_countGeometryChanges() {

            var stack = countGeometryChanges_Component.createObject(container)
            compare(stack.currentIndex, 0)
            compare(stack.col.width, 100)
            compare(stack.col.height, 110)
            compare(stack.row.width, 100)
            compare(stack.row.height, 100)
            verify(stack.col.r1.counter <= 2)
            compare(stack.col.r2.counter, 1)
            verify(stack.col.r3.counter <= 2)
            verify(stack.col.counter <= 2)
            compare(stack.row.counter, 1)    // not visible, will only receive the initial geometry change
            compare(stack.row.r5.counter, 0)
            stack.destroy()
        }

        Component {
            id: layoutItem_Component
            Rectangle {
                implicitWidth: 20
                implicitHeight: 20
            }
        }

        Component {
            id: emtpy_StackLayout_Component
            StackLayout {
                property int num_onCountChanged: 0
                property int num_onCurrentIndexChanged: 0
                onCountChanged: { ++num_onCountChanged; }
                onCurrentIndexChanged: { ++num_onCurrentIndexChanged; }
            }
        }

        function test_addAndRemoveItems()
        {
            var stack = emtpy_StackLayout_Component.createObject(container)
            stack.currentIndex = 2
            compare(stack.implicitWidth, 0)
            compare(stack.implicitHeight, 0)

            var rect0 = layoutItem_Component.createObject(stack)
            compare(stack.implicitWidth, 20)
            compare(stack.implicitHeight, 20)
            compare(rect0.visible, false)

            var rect1 = layoutItem_Component.createObject(stack)
            rect1.Layout.preferredWidth = 30
            rect1.Layout.preferredHeight = 10
            compare(stack.implicitWidth, 30)
            compare(stack.implicitHeight, 20)
            compare(rect0.visible, false)
            compare(rect1.visible, false)

            var rect2 = layoutItem_Component.createObject(stack)
            rect2.x = 42    // ### items in a stacklayout will have their x and y positions discarded.
            rect2.y = 42
            rect2.Layout.preferredWidth = 80
            rect2.Layout.preferredHeight = 30
            rect2.Layout.fillWidth = true
            compare(stack.implicitWidth, 80)
            compare(stack.implicitHeight, 30)
            compare(rect0.visible, false)
            compare(rect1.visible, false)
            compare(rect2.visible, true)
            compare(geometry(rect2), geometry(stack))

            rect2.destroy()
            wait(0)     // this will hopefully effectuate the destruction of the object
            compare(stack.implicitWidth, 30)
            compare(stack.implicitHeight, 20)

            rect0.destroy()
            wait(0)
            compare(stack.implicitWidth, 30)
            compare(stack.implicitHeight, 10)

            rect1.destroy()
            wait(0)
            compare(stack.implicitWidth, 0)
            compare(stack.implicitHeight, 0)

            stack.destroy()
        }

        function test_sizeHint_data() {
            return [
                    { tag: "propagateNone",            layoutHints: [10, 20, 30], childHints: [11, 21, 31], expected:[10, 20, Number.POSITIVE_INFINITY]},
                    { tag: "propagateMinimumWidth",    layoutHints: [-1, 20, 30], childHints: [10, 21, 31], expected:[10, 20, Number.POSITIVE_INFINITY]},
                    { tag: "propagatePreferredWidth",  layoutHints: [10, -1, 30], childHints: [11, 20, 31], expected:[10, 20, Number.POSITIVE_INFINITY]},
                    { tag: "propagateMaximumWidth",    layoutHints: [10, 20, -1], childHints: [11, 21, 30], expected:[10, 20, Number.POSITIVE_INFINITY]},
                    { tag: "propagateAll",             layoutHints: [-1, -1, -1], childHints: [10, 20, 30], expected:[10, 20, Number.POSITIVE_INFINITY]},
                    { tag: "propagateCrazy",           layoutHints: [-1, -1, -1], childHints: [40, 21, 30], expected:[30, 30, Number.POSITIVE_INFINITY]},
                    { tag: "expandMinToExplicitPref",  layoutHints: [-1,  1, -1], childHints: [11, 21, 31], expected:[ 1,  1, Number.POSITIVE_INFINITY]},
                    { tag: "expandMaxToExplicitPref",  layoutHints: [-1, 99, -1], childHints: [11, 21, 31], expected:[11, 99, Number.POSITIVE_INFINITY]},
                    { tag: "expandAllToExplicitMin",   layoutHints: [99, -1, -1], childHints: [11, 21, 31], expected:[99, 99, Number.POSITIVE_INFINITY]},
                    { tag: "expandPrefToExplicitMin",  layoutHints: [24, -1, -1], childHints: [11, 21, 31], expected:[24, 24, Number.POSITIVE_INFINITY]},
                    { tag: "boundPrefToExplicitMax",   layoutHints: [-1, -1, 19], childHints: [11, 21, 31], expected:[11, 19, Number.POSITIVE_INFINITY]},
                    { tag: "boundAllToExplicitMax",    layoutHints: [-1, -1,  9], childHints: [11, 21, 31], expected:[ 9,  9, Number.POSITIVE_INFINITY]},
                    ];
        }

        function itemSizeHints(item) {
            return [item.Layout.minimumWidth, item.implicitWidth, item.Layout.maximumWidth]
        }
        Component {
            id: stacklayout_sizeHint_Component
            StackLayout {
                property int implicitWidthChangedCount : 0
                onImplicitWidthChanged: { ++implicitWidthChangedCount }
                ColumnLayout {
                    Rectangle {
                        id: r1
                        color: "red"
                        Layout.minimumWidth: 1
                        Layout.preferredWidth: 2
                        Layout.maximumWidth: 3

                        Layout.minimumHeight: 20
                        Layout.preferredHeight: 20
                        Layout.maximumHeight: 20
                        Layout.fillWidth: true
                    }
                }
            }
        }

        function test_sizeHint(data) {
            var layout = stacklayout_sizeHint_Component.createObject(container)

            var col = layout.children[0]
            col.Layout.minimumWidth = data.layoutHints[0]
            col.Layout.preferredWidth = data.layoutHints[1]
            col.Layout.maximumWidth = data.layoutHints[2]

            var child = col.children[0]
            if (data.implicitWidth !== undefined) {
                child.implicitWidth = data.implicitWidth
            }
            child.Layout.minimumWidth = data.childHints[0]
            child.Layout.preferredWidth = data.childHints[1]
            child.Layout.maximumWidth = data.childHints[2]

            var effectiveSizeHintResult = [layout.Layout.minimumWidth, layout.implicitWidth, layout.Layout.maximumWidth]
            compare(effectiveSizeHintResult, data.expected)
            layout.destroy()
        }

        Component {
            id: stacklayout_addIgnoredItem_Component
            StackLayout {
                Repeater {
                    id: rep
                    model: 1
                    Rectangle {
                        id: r
                    }
                }
            }
        }

        // Items with no size information is ignored.
        function test_addIgnoredItem()
        {
            var stack = stacklayout_addIgnoredItem_Component.createObject(container)
            compare(stack.count, 1)
            compare(stack.implicitWidth, 0)
            compare(stack.implicitHeight, 0)
            var r = stack.children[0]
            r.Layout.preferredWidth = 20
            r.Layout.preferredHeight = 30
            compare(stack.count, 1)
            compare(stack.implicitWidth, 20)
            compare(stack.implicitHeight, 30)
            stack.destroy();
        }

        function test_dontCrashWhenAnchoredToAWindow() {
            var test_layoutStr =
           'import QtQuick 2.2;                         \
            import QtQuick.Window 2.1;                  \
            import QtQuick.Layouts 1.3;                 \
            Window {                                    \
                visible: true;                          \
                width: stack.implicitWidth;             \
                height: stack.implicitHeight;           \
                StackLayout {                           \
                    id: stack;                          \
                    currentIndex: 0;                    \
                    anchors.fill: parent;               \
                    Rectangle {                         \
                        color: "red";                   \
                        implicitWidth: 300;             \
                        implicitHeight: 200;            \
                    }                                   \
                }                                       \
            }                                           '

            var lay = Qt.createQmlObject(test_layoutStr, container, '');
            tryCompare(lay, 'width', 300);
            lay.destroy()
        }

        Component {
            id: test_dontCrashWhenChildIsResizedToNull_Component
            StackLayout {
                property alias rect : _rect
                Rectangle {
                    id: _rect;
                    color: "red"
                    implicitWidth: 200
                    implicitHeight: 200
                }
            }
        }

        function test_dontCrashWhenChildIsResizedToNull() {
            var layout = test_dontCrashWhenChildIsResizedToNull_Component.createObject(container)
            layout.rect.width = 0
            layout.width = 222      // trigger a rearrange with a valid size
            layout.height = 222
        }

        Component {
            id: test_currentIndex_Component
            StackLayout {
                currentIndex: 1
                Text {
                    text: "0"
                }
                Text {
                    text: "1"
                }
            }
        }

        function test_currentIndex() {
            var layout = test_currentIndex_Component.createObject(container)
            var c0 = layout.children[0]
            var c1 = layout.children[1]
            compare(layout.currentIndex, 1)
            tryCompare(layout, 'visible', true)
            compare(c0.visible, false)
            compare(c1.visible, true)
            layout.currentIndex = 0
            compare(c0.visible, true)
            compare(c1.visible, false)
            var c2 = layoutItem_Component.createObject(layout)
            compare(c2.visible, false)

            /*
             * destroy the current item and check if visibility advances to next
             */
            c0.destroy()
            tryCompare(c1, 'visible', true)
            compare(c2.visible, false)
            c1.destroy()
            tryCompare(c2, 'visible', true)
            c2.destroy()
            tryCompare(layout, 'currentIndex', 0)

            layout.destroy()

            /*
             * Test the default/implicit value of currentIndex, either -1 (if empty) or 0:
             */
            layout = emtpy_StackLayout_Component.createObject(container)
            tryCompare(layout, 'visible', true)
            compare(layout.currentIndex, -1)
            compare(layout.num_onCurrentIndexChanged, 0)
            // make it non-empty
            c0 = layoutItem_Component.createObject(layout)
            compare(layout.currentIndex, 0)
            compare(layout.num_onCurrentIndexChanged, 1)
            compare(c0.visible, true)
            // make it empty again
            c0.destroy()
            wait(0)
            compare(layout.currentIndex, -1)
            //tryCompare(layout, 'currentIndex', -1)
            compare(layout.num_onCurrentIndexChanged, 2)

            /*
             * Check that explicit value doesn't change,
             * and that no items are visible if the index is invalid/out of range
             */
            layout.currentIndex = 2
            compare(layout.currentIndex, 2)
            compare(layout.num_onCurrentIndexChanged, 3)
            c0 = layoutItem_Component.createObject(layout)
            compare(layout.currentIndex, 2)
            compare(c0.visible, false)

            c1 = layoutItem_Component.createObject(layout)
            compare(layout.currentIndex, 2)
            compare(c0.visible, false)
            compare(c1.visible, false)

            c2 = layoutItem_Component.createObject(layout)
            compare(layout.currentIndex, 2)
            compare(c0.visible, false)
            compare(c1.visible, false)
            compare(c2.visible, true)

            c2.destroy()
            wait(0)
            compare(layout.currentIndex, 2)
            compare(c0.visible, false)
            compare(c1.visible, false)
            c1.destroy()
            wait(0)
            compare(layout.currentIndex, 2)
            compare(c0.visible, false)
            c0.destroy()
            wait(0)
            compare(layout.currentIndex, 2)
            compare(layout.num_onCurrentIndexChanged, 3)
        }

        function test_count() {
            var layout = emtpy_StackLayout_Component.createObject(container)
            tryCompare(layout, 'visible', true)
            compare(layout.count, 0)
            compare(layout.currentIndex, -1)
            compare(layout.num_onCountChanged, 0)
            compare(layout.num_onCurrentIndexChanged, 0)
            var c0 = layoutItem_Component.createObject(layout)
            compare(layout.count, 1)
            compare(layout.currentIndex, 0)
            compare(layout.num_onCurrentIndexChanged, 1)
            compare(layout.num_onCountChanged, 1)
        }
    }
}
