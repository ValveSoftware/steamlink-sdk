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
import QtQuick.Layouts 1.0

Item {
    id: container
    width: 200
    height: 200
    TestCase {
        id: testCase
        name: "Tests_RowLayout"
        when: windowShown
        width: 200
        height: 200

        function itemRect(item)
        {
            return [item.x, item.y, item.width, item.height];
        }

        function test_fixedAndExpanding() {
            var test_layoutStr =
               'import QtQuick 2.2;                     \
                import QtQuick.Layouts 1.0;             \
                RowLayout {                             \
                    id: row;                            \
                    width: 15;                          \
                    spacing: 0;                         \
                    property alias r1: _r1;             \
                    Rectangle {                         \
                        id: _r1;                        \
                        width: 5;                       \
                        height: 10;                     \
                        color: "#8080ff";               \
                        Layout.fillWidth: false         \
                    }                                   \
                    property alias r2: _r2;             \
                    Rectangle {                         \
                        id: _r2;                        \
                        width: 10;                      \
                        height: 20;                     \
                        color: "#c0c0ff";               \
                        Layout.fillWidth: true          \
                    }                                   \
                }                                       '

            var lay = Qt.createQmlObject(test_layoutStr, container, '');
            tryCompare(lay, 'implicitWidth', 15);
            compare(lay.implicitHeight, 20);
            compare(lay.height, 20);
            lay.width = 30
            compare(lay.r1.x, 0);
            compare(lay.r1.width, 5);
            compare(lay.r2.x, 5);
            compare(lay.r2.width, 25);
            lay.destroy()
        }

        function test_allExpanding() {
            var test_layoutStr =
               'import QtQuick 2.2;                     \
                import QtQuick.Layouts 1.0;             \
                RowLayout {                             \
                    id: row;                            \
                    width: 15;                          \
                    spacing: 0;                         \
                    property alias r1: _r1;             \
                    Rectangle {                         \
                        id: _r1;                        \
                        width: 5;                       \
                        height: 10;                     \
                        color: "#8080ff";               \
                        Layout.fillWidth: true          \
                    }                                   \
                    property alias r2: _r2;             \
                    Rectangle {                         \
                        id: _r2;                        \
                        width: 10;                      \
                        height: 20;                     \
                        color: "#c0c0ff";               \
                        Layout.fillWidth: true          \
                    }                                   \
                }                                       '

            var tmp = Qt.createQmlObject(test_layoutStr, container, '');
            tryCompare(tmp, 'implicitWidth', 15);
            compare(tmp.implicitHeight, 20);
            compare(tmp.height, 20);
            tmp.width = 30
            compare(tmp.r1.width, 10);
            compare(tmp.r2.width, 20);
            compare(tmp.Layout.minimumWidth, 0)
            compare(tmp.Layout.maximumWidth, Number.POSITIVE_INFINITY)
            tmp.destroy()
        }

        function test_initialNestedLayouts() {
            var test_layoutStr =
               'import QtQuick 2.2;                             \
                import QtQuick.Layouts 1.0;                     \
                ColumnLayout {                                  \
                    id : col;                                   \
                    property alias row: _row;                   \
                    objectName: "col";                          \
                    anchors.fill: parent;                       \
                    RowLayout {                                 \
                        id : _row;                              \
                        property alias r1: _r1;                 \
                        property alias r2: _r2;                 \
                        objectName: "row";                      \
                        spacing: 0;                             \
                        Rectangle {                             \
                            id: _r1;                            \
                            color: "red";                       \
                            implicitWidth: 50;                  \
                            implicitHeight: 20;                 \
                        }                                       \
                        Rectangle {                             \
                            id: _r2;                            \
                            color: "green";                     \
                            implicitWidth: 50;                  \
                            implicitHeight: 20;                 \
                            Layout.fillWidth: true;             \
                        }                                       \
                    }                                           \
                }                                               '
            var col = Qt.createQmlObject(test_layoutStr, container, '');
            tryCompare(col, 'width', 200);
            tryCompare(col.row, 'width', 200);
            tryCompare(col.row.r1, 'width', 50);
            tryCompare(col.row.r2, 'width', 150);
            col.destroy()
        }

        function test_implicitSize() {
            var test_layoutStr =
               'import QtQuick 2.2;                             \
                import QtQuick.Layouts 1.0;                     \
                RowLayout {                                     \
                    id: row;                                    \
                    objectName: "row";                          \
                    spacing: 0;                                 \
                    height: 30;                                 \
                    anchors.left: parent.left;                  \
                    anchors.right: parent.right;                \
                    Rectangle {                                 \
                        color: "red";                           \
                        height: 2;                              \
                        Layout.minimumWidth: 50;                \
                    }                                           \
                    Rectangle {                                 \
                        color: "green";                         \
                        width: 10;                              \
                        Layout.minimumHeight: 4;                \
                    }                                           \
                    Rectangle {                                 \
                        implicitWidth: 1000;                    \
                        Layout.maximumWidth: 40;                \
                        implicitHeight: 6                       \
                    }                                           \
                }                                               '
            var row = Qt.createQmlObject(test_layoutStr, container, '');
            compare(row.implicitWidth, 50 + 10 + 40);
            compare(row.implicitHeight, 6);
            row.destroy()
        }

        function test_countGeometryChanges() {
            var test_layoutStr =
               'import QtQuick 2.2;                             \
                import QtQuick.Layouts 1.0;                     \
                ColumnLayout {                                  \
                    id : col;                                   \
                    property alias row: _row;                   \
                    objectName: "col";                          \
                    anchors.fill: parent;                       \
                    RowLayout {                                 \
                        id : _row;                              \
                        property alias r1: _r1;                 \
                        property alias r2: _r2;                 \
                        objectName: "row";                      \
                        spacing: 0;                             \
                        property int counter : 0;               \
                        onWidthChanged: { ++counter; }          \
                        Rectangle {                             \
                            id: _r1;                            \
                            color: "red";                       \
                            implicitWidth: 50;                  \
                            implicitHeight: 20;                 \
                            property int counter : 0;           \
                            onWidthChanged: { ++counter; }      \
                            Layout.fillWidth: true;             \
                        }                                       \
                        Rectangle {                             \
                            id: _r2;                            \
                            color: "green";                     \
                            implicitWidth: 50;                  \
                            implicitHeight: 20;                 \
                            property int counter : 0;           \
                            onWidthChanged: { ++counter; }      \
                            Layout.fillWidth: true;             \
                        }                                       \
                    }                                           \
                }                                               '
            var col = Qt.createQmlObject(test_layoutStr, container, '');
            compare(col.width, 200);
            compare(col.row.width, 200);
            compare(col.row.r1.width, 100);
            compare(col.row.r2.width, 100);
            compare(col.row.r1.counter, 1);
            compare(col.row.r2.counter, 1);
            verify(col.row.counter <= 2);
            col.destroy()
        }

        Component {
            id: layoutItem_Component
            Rectangle {
                implicitWidth: 20
                implicitHeight: 20
            }
        }

        Component {
            id: columnLayoutItem_Component
            ColumnLayout {
                spacing: 0
            }
        }

        Component {
            id: layout_addAndRemoveItems_Component
            RowLayout {
                spacing: 0
            }
        }

        function test_addAndRemoveItems()
        {
            var layout = layout_addAndRemoveItems_Component.createObject(container)
            compare(layout.implicitWidth, 0)
            compare(layout.implicitHeight, 0)

            var rect0 = layoutItem_Component.createObject(layout)
            compare(layout.implicitWidth, 20)
            compare(layout.implicitHeight, 20)

            var rect1 = layoutItem_Component.createObject(layout)
            rect1.Layout.preferredWidth = 30;
            rect1.Layout.preferredHeight = 30;
            compare(layout.implicitWidth, 50)
            compare(layout.implicitHeight, 30)

            var col = columnLayoutItem_Component.createObject(layout)
            var rect2 = layoutItem_Component.createObject(col)
            rect2.Layout.fillHeight = true
            var rect3 = layoutItem_Component.createObject(col)
            rect3.Layout.fillHeight = true

            compare(layout.implicitWidth, 70)
            compare(col.implicitHeight, 40)
            compare(layout.implicitHeight, 40)

            rect3.destroy()
            wait(0)     // this will hopefully effectuate the destruction of the object

            col.destroy()
            wait(0)
            compare(layout.implicitWidth, 50)
            compare(layout.implicitHeight, 30)

            rect0.destroy()
            wait(0)
            compare(layout.implicitWidth, 30)
            compare(layout.implicitHeight, 30)

            rect1.destroy()
            wait(0)
            compare(layout.implicitWidth, 0)
            compare(layout.implicitHeight, 0)

            layout.destroy()
        }

        Component {
            id: layout_alignment_Component
            RowLayout {
                spacing: 0
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.fillHeight: true
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    // use default alignment
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignTop
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignBottom
                }
            }
        }

        function test_alignment()
        {
            var layout = layout_alignment_Component.createObject(container);
            layout.width = 100;
            layout.height = 40;

            compare(itemRect(layout.children[0]), [ 0,  0, 20, 40]);
            compare(itemRect(layout.children[1]), [20, 10, 20, 20]);
            compare(itemRect(layout.children[2]), [40,  0, 20, 20]);
            compare(itemRect(layout.children[3]), [60, 10, 20, 20]);
            compare(itemRect(layout.children[4]), [80, 20, 20, 20]);
            layout.destroy();
        }

        Component {
            id: layout_sizeHintNormalization_Component
            GridLayout {
                columnSpacing: 0
                rowSpacing: 0
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

        function test_sizeHintNormalization_data() {
            return [
                    { tag: "fallbackValues",  widthHints: [-1, -1, -1], implicitWidth: 42, expected:[0,42,Number.POSITIVE_INFINITY]},
                    { tag: "acceptZeroWidths",  widthHints: [0, 0, 0], implicitWidth: 42, expected:[0,0,0]},
                    { tag: "123",  widthHints: [1,2,3],  expected:[1,2,3]},
                    { tag: "132",  widthHints: [1,3,2],  expected:[1,2,2]},
                    { tag: "213",  widthHints: [2,1,3],  expected:[2,2,3]},
                    { tag: "231",  widthHints: [2,3,1],  expected:[1,1,1]},
                    { tag: "321",  widthHints: [3,2,1],  expected:[1,1,1]},
                    { tag: "312",  widthHints: [3,1,2],  expected:[2,2,2]},

                    { tag: "1i3",  widthHints: [1,-1,3], implicitWidth: 2,  expected:[1,2,3]},
                    { tag: "1i2",  widthHints: [1,-1,2], implicitWidth: 3,  expected:[1,2,2]},
                    { tag: "2i3",  widthHints: [2,-1,3], implicitWidth: 1,  expected:[2,2,3]},
                    { tag: "2i1",  widthHints: [2,-1,1], implicitWidth: 3,  expected:[1,1,1]},
                    { tag: "3i1",  widthHints: [3,-1,1], implicitWidth: 2,  expected:[1,1,1]},
                    { tag: "3i2",  widthHints: [3,-1,2], implicitWidth: 1,  expected:[2,2,2]},
                    ];
        }

        function test_sizeHintNormalization(data) {
            var layout = layout_sizeHintNormalization_Component.createObject(container);
            if (data.implicitWidth !== undefined) {
                layout.children[0].implicitWidth = data.implicitWidth
            }
            layout.children[0].Layout.minimumWidth = data.widthHints[0];
            layout.children[0].Layout.preferredWidth = data.widthHints[1];
            layout.children[0].Layout.maximumWidth = data.widthHints[2];
            wait(0);    // Trigger processEvents() (allow LayoutRequest to be processed)
            var normalizedResult = [layout.Layout.minimumWidth, layout.implicitWidth, layout.Layout.maximumWidth]
            compare(normalizedResult, data.expected);
            layout.destroy();
        }

        Component {
            id: layout_sizeHint_Component
            RowLayout {
                property int implicitWidthChangedCount : 0
                onImplicitWidthChanged: { ++implicitWidthChangedCount }
                GridLayout {
                    columnSpacing: 0
                    rowSpacing: 0
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

        function test_sizeHint_data() {
            return [
                    { tag: "propagateNone",            layoutHints: [10, 20, 30], childHints: [11, 21, 31], expected:[10, 20, 30]},
                    { tag: "propagateMinimumWidth",    layoutHints: [-1, 20, 30], childHints: [10, 21, 31], expected:[10, 20, 30]},
                    { tag: "propagatePreferredWidth",  layoutHints: [10, -1, 30], childHints: [11, 20, 31], expected:[10, 20, 30]},
                    { tag: "propagateMaximumWidth",    layoutHints: [10, 20, -1], childHints: [11, 21, 30], expected:[10, 20, 30]},
                    { tag: "propagateAll",             layoutHints: [-1, -1, -1], childHints: [10, 20, 30], expected:[10, 20, 30]},
                    { tag: "propagateCrazy",           layoutHints: [-1, -1, -1], childHints: [40, 21, 30], expected:[30, 30, 30]},
                    { tag: "expandMinToExplicitPref",  layoutHints: [-1,  1, -1], childHints: [11, 21, 31], expected:[ 1,  1, 31]},
                    { tag: "expandMaxToExplicitPref",  layoutHints: [-1, 99, -1], childHints: [11, 21, 31], expected:[11, 99, 99]},
                    { tag: "expandAllToExplicitMin",   layoutHints: [99, -1, -1], childHints: [11, 21, 31], expected:[99, 99, 99]},
                    { tag: "expandPrefToExplicitMin",  layoutHints: [24, -1, -1], childHints: [11, 21, 31], expected:[24, 24, 31]},
                    { tag: "boundPrefToExplicitMax",   layoutHints: [-1, -1, 19], childHints: [11, 21, 31], expected:[11, 19, 19]},
                    { tag: "boundAllToExplicitMax",    layoutHints: [-1, -1,  9], childHints: [11, 21, 31], expected:[ 9,  9,  9]},
                    ];
        }

        function itemSizeHints(item) {
            return [item.Layout.minimumWidth, item.implicitWidth, item.Layout.maximumWidth]
        }

        function test_sizeHint(data) {
            var layout = layout_sizeHint_Component.createObject(container)

            var grid = layout.children[0]
            grid.Layout.minimumWidth = data.layoutHints[0]
            grid.Layout.preferredWidth = data.layoutHints[1]
            grid.Layout.maximumWidth = data.layoutHints[2]

            var child = grid.children[0]
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

        function test_sizeHintPropagationCount() {
            var layout = layout_sizeHint_Component.createObject(container)
            var child = layout.children[0].children[0]

            child.Layout.minimumWidth = -1
            compare(itemSizeHints(layout), [0, 2, 3])
            child.Layout.preferredWidth = -1
            compare(itemSizeHints(layout), [0, 0, 3])
            child.Layout.maximumWidth = -1
            compare(itemSizeHints(layout), [0, 0, Number.POSITIVE_INFINITY])
            layout.Layout.maximumWidth = 1000
            compare(itemSizeHints(layout), [0, 0, 1000])
            layout.Layout.maximumWidth = -1
            compare(itemSizeHints(layout), [0, 0, Number.POSITIVE_INFINITY])

            layout.implicitWidthChangedCount = 0
            child.Layout.minimumWidth = 10
            compare(itemSizeHints(layout), [10, 10, Number.POSITIVE_INFINITY])
            compare(layout.implicitWidthChangedCount, 1)

            child.Layout.preferredWidth = 20
            compare(itemSizeHints(layout), [10, 20, Number.POSITIVE_INFINITY])
            compare(layout.implicitWidthChangedCount, 2)

            child.Layout.maximumWidth = 30
            compare(itemSizeHints(layout), [10, 20, 30])
            compare(layout.implicitWidthChangedCount, 2)

            child.Layout.maximumWidth = 15
            compare(itemSizeHints(layout), [10, 15, 15])
            compare(layout.implicitWidthChangedCount, 3)

            child.Layout.maximumWidth = 30
            compare(itemSizeHints(layout), [10, 20, 30])
            compare(layout.implicitWidthChangedCount, 4)

            layout.Layout.maximumWidth = 29
            compare(layout.Layout.maximumWidth, 29)
            layout.Layout.maximumWidth = -1
            compare(layout.Layout.maximumWidth, 30)

            layout.destroy()
        }

        Component {
            id: layout_change_implicitWidth_during_rearrange
            ColumnLayout {
                width: 100
                height: 20
                RowLayout {
                    spacing: 0
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: false
                        implicitWidth: height
                        color: "red"
                    }
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: "blue"
                    }
                }
            }
        }

        function test_change_implicitWidth_during_rearrange() {
            var layout = layout_change_implicitWidth_during_rearrange.createObject(container)
            var red = layout.children[0].children[0]
            var blue = layout.children[0].children[1]
            waitForRendering(layout);
            tryCompare(red, 'width', 20)
            tryCompare(blue, 'width', 80)
            layout.height = 40
            tryCompare(red, 'width', 40)
            tryCompare(blue, 'width', 60)
            layout.destroy()
        }

        Component {
            id: layout_addIgnoredItem_Component
            RowLayout {
                spacing: 0
                Rectangle {
                    id: r
                }
            }
        }

        function test_addIgnoredItem()
        {
            var layout = layout_addIgnoredItem_Component.createObject(container)
            compare(layout.implicitWidth, 0)
            compare(layout.implicitHeight, 0)
            var r = layout.children[0]
            r.Layout.preferredWidth = 20
            r.Layout.preferredHeight = 30
            compare(layout.implicitWidth, 20)
            compare(layout.implicitHeight, 30)

            layout.destroy();
        }


        Component {
            id: layout_rowLayout_Component
            RowLayout {
            }
        }

        function test_stretchItem_data()
        {
            return [
                    { expectedWidth: 0},
                    { preferredWidth: 20, expectedWidth: 20},
                    { preferredWidth: 0, expectedWidth: 0},
                    { preferredWidth: 20, fillWidth: true, expectedWidth: 100},
                    { width: 20, fillWidth: true, expectedWidth: 100},
                    { width: 0, fillWidth: true, expectedWidth: 100},
                    { preferredWidth: 0, fillWidth: true, expectedWidth: 100},
                    { preferredWidth: 1, maximumWidth: 0, fillWidth: true, expectedWidth: 0},
                    { preferredWidth: 0, minimumWidth: 1, expectedWidth: 1},
                    ];
        }

        function test_stretchItem(data)
        {
            var layout = layout_rowLayout_Component.createObject(container)
            var r = layoutItem_Component.createObject(layout)
            // Reset previously relevant properties
            r.width = 0
            r.implicitWidth = 0
            compare(layout.implicitWidth, 0)

            if (data.preferredWidth !== undefined)
                r.Layout.preferredWidth = data.preferredWidth
            if (data.fillWidth !== undefined)
                r.Layout.fillWidth = data.fillWidth
            if (data.width !== undefined)
                r.width = data.width
            if (data.minimumWidth !== undefined)
                r.Layout.minimumWidth = data.minimumWidth
            if (data.maximumWidth !== undefined)
                r.Layout.maximumWidth = data.maximumWidth

            layout.width = 100

            compare(r.width, data.expectedWidth)

            layout.destroy();
        }

        Component {
            id: layout_alignToPixelGrid_Component
            RowLayout {
                spacing: 2
                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    Layout.alignment: Qt.AlignVCenter
                }
                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
        function test_alignToPixelGrid()
        {
            var layout = layout_alignToPixelGrid_Component.createObject(container)
            layout.width  = 21
            layout.height = 21
            var r0 = layout.children[0]
            compare(r0.x, 0) // 0.0
            compare(r0.y, 6) // 5.5
            var r1 = layout.children[1]
            compare(r1.x, 12) // 11.5
            compare(r1.y, 6) // 5.5
            layout.destroy();
        }

        Component {
            id: test_distributeToPixelGrid_Component
            RowLayout {
                spacing: 0
            }
        }

        function test_distributeToPixelGrid_data() {
            return [
                    { tag: "narrow",  spacing: 0, width: 60, hints: [{pref: 50}, {pref: 20}, {pref: 70}] },
                    { tag: "belowPreferred",  spacing: 0, width: 130, hints: [{pref: 50}, {pref: 20}, {pref: 70}]},
                    { tag: "belowPreferredWithSpacing", spacing: 10, width: 130, hints: [{pref: 50}, {pref: 20}, {pref: 70}]},
                    { tag: "abovePreferred",  spacing: 0, width: 150, hints: [{pref: 50}, {pref: 20}, {pref: 70}]},
                    { tag: "stretchSomethingToMaximum",  spacing: 0, width: 240, hints: [{pref: 50}, {pref: 20}, {pref: 70}],
                      expected: [90, 60, 90] },
                    { tag: "minSizeHasFractions",  spacing: 2, width: 33 + 4, hints: [{min: 10+1/3}, {min: 10+1/3}, {min: 10+1/3}],
                      /*expected: [11, 11, 11]*/ },     /* verify that nothing gets allocated a size smaller than its minimum */
                    { tag: "maxSizeHasFractions",  spacing: 2, width: 271 + 4, hints: [{max: 90+1/3}, {max: 90+1/3}, {max: 90+1/3}],
                      /*expected: [90, 90, 90]*/ },     /* verify that nothing gets allocated a size larger than its maximum */
                    { tag: "fixedSizeHasFractions",  spacing: 2, width: 31 + 4, hints: [{min: 10+1/3, max: 10+1/3}, {min: 10+1/3, max: 10+1/3}, {min: 10+1/3, max: 10+1/3}],
                      /*expected: [11, 11, 11]*/ },     /* verify that nothing gets allocated a size smaller than its minimum */
                    { tag: "481", spacing: 0, width: 481,
                      hints: [{min:0, pref:0, max:999}, {min:0, pref:0, max: 999}, {min: 0, pref: 0, max:0}],
                      expected: [241, 240, 0] },
                    { tag: "theend", spacing: 1, width: 18,
                      hints: [{min: 10, pref: 10, max:10}, {min:3, pref:3.33}, {min:2, pref:2.33}],
                      expected: [10, 4, 2] },
                    { tag: "theend2",  spacing: 1, width: 18,
                      hints: [{min: 10, pref: 10, max:10}, {min:3, pref:3.33}, {min:2.33, pref:2.33}],
                      expected: [10, 3, 3] },
                    { tag: "43",  spacing: 0, width: 43,
                      hints: [{min: 10, pref: 10, max:10}, {min:10, pref:30.33}, {min:2.33, pref:2.33}],
                      expected: [10, 30, 3] },
                    { tag: "40",  spacing: 0, width: 40,
                      hints: [{min: 10, pref: 10, max:10}, {min:10, pref:30.33}, {min:2.33, pref:2.33}],
                      expected: [10, 27, 3] },
                    { tag: "roundingAccumulates1",  spacing: 0, width: 50,
                      hints: [{pref: 10, max:30.3},
                              {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3},
                              {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3},
                              {pref: 10, max:30.3}],
                      expected: [10,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   10] },
                    { tag: "roundingAccumulates2",  spacing: 0, width: 60,
                      hints: [{pref: 20, max:30.3},
                              {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3},
                              {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3}, {min:2.3, pref:2.3},
                              {pref: 20, max:30.3}],
                      expected: [15,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   15] },
                    ];
        }

        function test_distributeToPixelGrid(data)
        {
            // CONFIGURATION
            var layout = test_distributeToPixelGrid_Component.createObject(container)
            layout.spacing = data.spacing
            layout.width  = data.width
            layout.height = 10

            var hints = data.hints
            var i;
            var n = hints.length
            for (i = 0; i < n; ++i) {
                var rect = layoutItem_Component.createObject(layout)
                rect.Layout.fillWidth = true
                var h = hints[i]
                rect.Layout.minimumWidth = h.hasOwnProperty('min') ? h.min : 10
                if (h.hasOwnProperty('pref'))
                    rect.Layout.preferredWidth = h.pref
                rect.Layout.maximumWidth = h.hasOwnProperty('max') ? h.max : 90
            }

            var kids = layout.children

            waitForRendering(layout)

            var sum = (n - 1) * layout.spacing
            // TEST
            for (i = 0; i < n; ++i) {
                compare(kids[i].x % 1, 0)           // checks if position is a whole integer
                // check if width is a whole integer (unless there are constraints preventing it from stretching)
                verify(kids[i].width % 1 == 0
                       || Math.floor(kids[i].Layout.maximumWidth) < kids[i].width
                       || layout.width < layout.Layout.maximumWidth + 1)
                // verify if the items are within the size constraints as specified
                verify(kids[i].width >= kids[i].Layout.minimumWidth)
                verify(kids[i].width <= kids[i].Layout.maximumWidth)
                if (data.hasOwnProperty('expected'))
                    compare(kids[i].width,  data.expected[i])
                sum += kids[i].width
            }
            fuzzyCompare(sum, layout.width, 1)

            layout.destroy();
        }



        Component {
            id: layout_deleteLayout
            ColumnLayout {
                property int dummyproperty: 0   // yes really - its needed
                RowLayout {
                    Text { text: "label1" }     // yes, both are needed
                    Text { text: "label2" }
                }
            }
        }

        function test_destroyLayout()
        {
            var layout = layout_deleteLayout.createObject(container)
            layout.children[0].children[0].visible = true
            layout.visible = false
            layout.destroy()    // Do not crash
        }

        function test_sizeHintWithHiddenChildren(data) {
            var layout = layout_sizeHint_Component.createObject(container)
            var grid = layout.children[0]
            var child = grid.children[0]

            // Implicit sizes are not affected by the visibility of the parent layout.
            // This is in order for the layout to know the preferred size it should show itself at.
            compare(grid.visible, true)     // LAYOUT SHOWN
            compare(grid.implicitWidth, 2);
            child.visible = false
            compare(grid.implicitWidth, 0);
            child.visible = true
            compare(grid.implicitWidth, 2);

            grid.visible = false            // LAYOUT HIDDEN
            compare(grid.implicitWidth, 2);
            child.visible = false
            expectFail('', 'If GridLayout is hidden, GridLayout is not notified when child is explicitly hidden')
            compare(grid.implicitWidth, 0);
            child.visible = true
            compare(grid.implicitWidth, 2);

            layout.destroy();
        }

        Component {
            id: row_sizeHint_Component
            Row {
                Rectangle {
                    id: r1
                    color: "red"
                    width: 2
                    height: 20
                }
            }
        }

        function test_sizeHintWithHiddenChildrenForRow(data) {
            var row = row_sizeHint_Component.createObject(container)
            var child = row.children[0]
            compare(row.visible, true)     // POSITIONER SHOWN
            compare(row.implicitWidth, 2);
            child.visible = false
            tryCompare(row, 'implicitWidth', 0);
            child.visible = true
            tryCompare(row, 'implicitWidth', 2);

            row.visible = false            // POSITIONER HIDDEN
            compare(row.implicitWidth, 2);
            child.visible = false
            expectFail('', 'If Row is hidden, Row is not notified when child is explicitly hidden')
            compare(row.implicitWidth, 0);
            child.visible = true
            compare(row.implicitWidth, 2);
        }

        Component {
            id: rearrangeNestedLayouts_Component
            RowLayout {
                id: layout
                anchors.fill: parent
                width: 200
                height: 20
                RowLayout {
                    id: row
                    spacing: 0

                    Rectangle {
                        id: fixed
                        color: 'red'
                        implicitWidth: 20
                        implicitHeight: 20
                    }
                    Rectangle {
                        id: filler
                        color: 'grey'
                        Layout.fillWidth: true
                        implicitHeight: 20
                    }
                }
            }
        }

        function test_rearrangeNestedLayouts()
        {
            var layout = rearrangeNestedLayouts_Component.createObject(container)
            var fixed = layout.children[0].children[0]
            var filler = layout.children[0].children[1]

            compare(itemRect(fixed),  [0,0,20,20])
            compare(itemRect(filler), [20,0,180,20])

            fixed.implicitWidth = 100
            waitForRendering(layout)
            compare(itemRect(fixed),  [0,0,100,20])
            compare(itemRect(filler), [100,0,100,20])
        }

        Component {
            id: changeChildrenOfHiddenLayout_Component
            RowLayout {
                property int childCount: 1
                Repeater {
                    model: parent.childCount
                    Text {
                        text: 'Just foo it'
                    }
                }
            }
        }
        function test_changeChildrenOfHiddenLayout()
        {
            var layout = changeChildrenOfHiddenLayout_Component.createObject(container)
            var child = layout.children[0]
            waitForRendering(layout)
            layout.visible = false
            waitForRendering(layout)
            // Remove and add children to the hidden layout..
            layout.childCount = 0
            waitForRendering(layout)
            layout.childCount = 1
            waitForRendering(layout)
            layout.destroy()
        }


        function test_defaultPropertyAliasCrash() {
            var containerUserComponent = Qt.createComponent("rowlayout/ContainerUser.qml");
            compare(containerUserComponent.status, Component.Ready);

            var containerUser = containerUserComponent.createObject(testCase);
            verify(containerUser);

            // Shouldn't crash.
            containerUser.destroy();
        }
    }
}
