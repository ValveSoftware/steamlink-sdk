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
import QtQuick.Layouts 1.1

Item {
    id: container
    width: 200
    height: 200
    TestCase {
        id: testCase
        name: "Tests_GridLayout"
        when: windowShown
        width: 200
        height: 200

        Component {
            id: layout_flow_Component
            GridLayout {
                columns: 4
                columnSpacing: 0
                rowSpacing: 0
                Repeater {
                    model: 6
                    Rectangle {
                        property var itemRect: [x, y, width, height]
                        color: "red"
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        Text { text: index }
                    }
                }
            }
        }

        function test_flow()
        {
            var layout = layout_flow_Component.createObject(container);
            tryCompare(layout.children[0], "itemRect", [ 0,  0, 10, 10])
            tryCompare(layout.children[1], "itemRect", [10,  0, 10, 10])
            tryCompare(layout.children[2], "itemRect", [20,  0, 10, 10])
            tryCompare(layout.children[3], "itemRect", [30,  0, 10, 10])

            tryCompare(layout.children[4], "itemRect", [ 0, 10, 10, 10])
            tryCompare(layout.children[5], "itemRect", [10, 10, 10, 10])

            layout.rows = 4
            layout.flow = GridLayout.TopToBottom
            tryCompare(layout.children[0], "itemRect", [ 0,  0, 10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0, 10, 10, 10])
            tryCompare(layout.children[2], "itemRect", [ 0, 20, 10, 10])
            tryCompare(layout.children[3], "itemRect", [ 0, 30, 10, 10])

            tryCompare(layout.children[4], "itemRect", [10,  0, 10, 10])
            tryCompare(layout.children[5], "itemRect", [10, 10, 10, 10])

            layout.destroy()
        }

        Component {
            id: layout_flowLeftToRight_Component
            GridLayout {
                columns: 4
                columnSpacing: 0
                rowSpacing: 0
                // red rectangles are auto-positioned
                // black rectangles are explicitly positioned with row,column
                Rectangle {
                    // First one should auto position itself at (0,0)
                    id: r1
                    color: "red"
                    width: 20
                    height: 20
                }
                Rectangle {
                    // (1,1)
                    id: r2
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 1
                    Layout.column: 1
                    Layout.rowSpan: 2
                    Layout.columnSpan: 2
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    // (0,1)
                    id: r3
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 0
                    Layout.column: 1
                }
                Rectangle {
                    // This one won't fit on the left and right sides of the big black box
                    // inserted at (3,0)
                    id: r4
                    color: "red"
                    width: 20
                    height: 20
                    Layout.columnSpan: 2
                    Layout.rowSpan: 2
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    // continue flow from (0,2)
                    id: r5
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 0
                    Layout.column: 2
                }
                Repeater {
                    // ...and let the rest of the items automatically fill in the empty cells
                    model: 8
                    Rectangle {
                        color: "red"
                        width: 20
                        height: 20
                        Text { text: index }
                    }
                }
            }
        }

        function test_flowLeftToRight() {
            var layout = layout_flowLeftToRight_Component.createObject(container);
            compare(layout.implicitWidth, 80);
            compare(layout.children[0].x, 0);
            compare(layout.children[0].y, 0);
            compare(layout.children[1].x, 20);
            compare(layout.children[1].y, 20);
            compare(layout.children[2].x, 20);
            compare(layout.children[2].y, 0);
            compare(layout.children[3].x, 0);
            compare(layout.children[3].y, 60);
            compare(layout.children[4].x, 40);
            compare(layout.children[4].y, 0);

            // assumes that the repeater is the last item among the items it creates
            compare(layout.children[5].x, 60);
            compare(layout.children[5].y, 00);
            compare(layout.children[6].x, 00);
            compare(layout.children[6].y, 20);
            compare(layout.children[7].x, 60);
            compare(layout.children[7].y, 20);
            compare(layout.children[8].x, 00);
            compare(layout.children[8].y, 40);
            compare(layout.children[9].x, 60);
            compare(layout.children[9].y, 40);
            compare(layout.children[10].x, 40);
            compare(layout.children[10].y, 60);
            compare(layout.children[11].x, 60);
            compare(layout.children[11].y, 60);
            compare(layout.children[12].x, 40);
            compare(layout.children[12].y, 80);

            layout.destroy();
        }


        Component {
            id: layout_flowLeftToRightDefaultPositions_Component
            GridLayout {
                columns: 2
                columnSpacing: 0
                rowSpacing: 0
                // red rectangles are auto-positioned
                // black rectangles are explicitly positioned with row,column
                // gray rectangles are items with just one row or just one column specified
                Rectangle {
                    // First one should auto position itself at (0,0)
                    id: r1
                    color: "red"
                    width: 20
                    height: 20
                }
                Rectangle {
                    // (1,0)
                    id: r2
                    color: "gray"
                    width: 20
                    height: 20
                    Layout.row: 1
                }
                Rectangle {
                    // (1,1)
                    id: r3
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 1
                    Layout.column: 1
                }
                Rectangle {
                    // (1,0), warning emitted
                    id: r4
                    color: "gray"
                    width: 20
                    height: 20
                    Layout.row: 1
                }
            }
        }

        function test_flowLeftToRightDefaultPositions() {
            ignoreWarning("QGridLayoutEngine::addItem: Cell (1, 0) already taken");
            var layout = layout_flowLeftToRightDefaultPositions_Component.createObject(container);
            compare(layout.implicitWidth, 40);
            compare(layout.children[0].x, 0);
            compare(layout.children[0].y, 0);
            compare(layout.children[1].x, 0);
            compare(layout.children[1].y, 20);
            compare(layout.children[2].x, 20);
            compare(layout.children[2].y, 20);
            layout.destroy();
        }


        Component {
            id: layout_flowTopToBottom_Component
            GridLayout {
                rows: 4
                columnSpacing: 0
                rowSpacing: 0
                flow: GridLayout.TopToBottom
                // red rectangles are auto-positioned
                // black rectangles are explicitly positioned with row,column
                Rectangle {
                    // First one should auto position itself at (0,0)
                    id: r1
                    color: "red"
                    width: 20
                    height: 20
                }
                Rectangle {
                    // (1,1)
                    id: r2
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 1
                    Layout.column: 1
                    Layout.rowSpan: 2
                    Layout.columnSpan: 2
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    // (2,0)
                    id: r3
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 2
                    Layout.column: 0
                }
                Rectangle {
                    // This one won't fit on the left and right sides of the big black box
                    // inserted at (0,3)
                    id: r4
                    color: "red"
                    width: 20
                    height: 20
                    Layout.rowSpan: 2
                    Layout.fillHeight: true
                }
                Rectangle {
                    // continue flow from (1,0)
                    id: r5
                    color: "black"
                    width: 20
                    height: 20
                    Layout.row: 1
                    Layout.column: 0
                }
                Repeater {
                    // ...and let the rest of the items automatically fill in the empty cells
                    model: 8
                    Rectangle {
                        color: "red"
                        width: 20
                        height: 20
                        Text { text: index }
                    }
                }
            }
        }

        function test_flowTopToBottom() {
            var layout = layout_flowTopToBottom_Component.createObject(container);
            compare(layout.children[0].x, 0);
            compare(layout.children[0].y, 0);
            compare(layout.children[1].x, 20);
            compare(layout.children[1].y, 20);
            compare(layout.children[2].x, 0);
            compare(layout.children[2].y, 40);
            compare(layout.children[3].x, 60);
            compare(layout.children[3].y, 0);
            compare(layout.children[4].x, 0);
            compare(layout.children[4].y, 20);

            // The repeated items
            compare(layout.children[5].x, 0);
            compare(layout.children[5].y, 60);
            compare(layout.children[6].x, 20);
            compare(layout.children[6].y, 0);
            compare(layout.children[7].x, 20);
            compare(layout.children[7].y, 60);
            compare(layout.children[8].x, 40);
            compare(layout.children[8].y, 0);
            compare(layout.children[9].x, 40);
            compare(layout.children[9].y, 60);
            compare(layout.children[10].x, 60);
            compare(layout.children[10].y, 40);
            compare(layout.children[11].x, 60);
            compare(layout.children[11].y, 60);
            compare(layout.children[12].x, 80);
            compare(layout.children[12].y, 0);

            layout.destroy();
        }

        Component {
            id: layout_spanAcrossEmptyRows_Component
            /* This test has a large number of empty rows and columns, but there is one item
               that spans across some of these empty rows/columns.
               Do not modify (especially do not add items unless you understand what this is
               testing)
             */

            GridLayout {
                columnSpacing: 0
                rowSpacing: 0
                // black rectangles are explicitly positioned with row,column
                Rectangle {
                    // (0,0)
                    id: r0
                    color: "black"
                    Layout.row: 0
                    Layout.column: 0
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.maximumWidth: 40
                    Layout.maximumHeight: 40
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Rectangle {
                    // (0,1)
                    id: r1
                    color: "black"
                    Layout.row: 0
                    Layout.column: 1
                    Layout.columnSpan: 2
                    Layout.rowSpan: 2
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.maximumWidth: 40
                    Layout.maximumHeight: 40
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Rectangle {
                    // (0,99)
                    id: r2
                    color: "black"
                    Layout.row: 0
                    Layout.column: 99
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.maximumWidth: 40
                    Layout.maximumHeight: 40
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }

        function test_spanAcrossEmptyRows() {
            var layout = layout_spanAcrossEmptyRows_Component.createObject(container);
            compare(layout.children[0].x, 0);
            compare(layout.children[0].y, 0);
            compare(layout.children[1].x, 20);
            compare(layout.children[1].y, 0);
            compare(layout.children[2].x, 40);
            compare(layout.children[2].y, 0);

            compare(layout.implicitWidth, 60);
            compare(layout.Layout.maximumWidth, 120);

            layout.destroy();
        }

        Component {
            id: layout_spanIsMoreThanColumns_Component

            GridLayout {
                columnSpacing: 1
                rowSpacing: 1
                columns: 2

                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    Layout.columnSpan: 3
                }
            }
        }

        function test_spanIsMoreThanColumns() {
            var layout = layout_spanIsMoreThanColumns_Component.createObject(container);
            // item was not added, therefore implicit width is 0
            compare(layout.implicitWidth, 0);
            layout.destroy();
        }

        function test_sizeHints() {
            var layout = layout_spanAcrossEmptyRows_Component.createObject(container);
            compare(layout.visible, true)

            var minWidth  = layout.Layout.minimumWidth
            var minHeight = layout.Layout.minimumHeight

            var prefWidth  = layout.implicitWidth
            var prefHeight = layout.implicitHeight

            var maxWidth  = layout.Layout.maximumWidth
            var maxHeight = layout.Layout.maximumHeight

            layout.visible = false
            compare(minWidth, layout.Layout.minimumWidth)
            compare(minHeight, layout.Layout.minimumHeight)
            compare(prefWidth, layout.implicitWidth)
            compare(prefHeight, layout.implicitHeight)
            compare(maxWidth, layout.Layout.maximumWidth)
            compare(maxHeight, layout.Layout.maximumHeight)

            layout.destroy();
        }

        Component {
            id: layout_alignment_Component
            GridLayout {
                columns: 2
                columnSpacing: 0
                rowSpacing: 0
                Rectangle {
                    // First one should auto position itself at (0,0)
                    property var itemRect: [x, y, width, height]
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Rectangle {
                    // (0,1)
                    property var itemRect: [x, y, width, height]
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignBottom
                }
                Rectangle {
                    // (1,0)
                    property var itemRect: [x, y, width, height]
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignRight
                }
                Rectangle {
                    // (1,1)
                    property var itemRect: [x, y, width, height]
                    color: "red"
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                }
                Rectangle {
                    // (2,0)
                    property var itemRect: [x, y, width, height]
                    color: "red"
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    Layout.alignment: Qt.AlignRight
                    Layout.columnSpan: 2
                }
                Rectangle {
                    // (3,0)
                    property var itemRect: [x, y, width, height]
                    baselineOffset: 7
                    color: "red"
                    Layout.row: 3
                    Layout.column: 0
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBaseline
                }
                Rectangle {
                    // (3,1)
                    property var itemRect: [x, y, width, height]
                    baselineOffset: 7
                    color: "red"
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                }

            }
        }

        function test_alignment()
        {
            var layout = layout_alignment_Component.createObject(container);
            layout.width = 60;
            layout.height = 100;


            tryCompare(layout.children[0], "itemRect", [ 0,  0, 40, 40]);
            tryCompare(layout.children[1], "itemRect", [40, 20, 20, 20]);
            tryCompare(layout.children[2], "itemRect", [20, 40, 20, 20]);
            tryCompare(layout.children[3], "itemRect", [45, 40, 10, 10]);
            tryCompare(layout.children[4], "itemRect", [30, 60, 30, 30]);
            tryCompare(layout.children[5], "itemRect", [ 0, 90, 10, 10]);
            tryCompare(layout.children[6], "itemRect", [50, 90, 10, 10]);


            layout.children[1].Layout.alignment = Qt.AlignTop
            tryCompare(layout.children[1], "x", 40);
            tryCompare(layout.children[1], "y", 0);

            layout.children[2].Layout.alignment = Qt.AlignLeft
            tryCompare(layout.children[2], "x", 0);
            tryCompare(layout.children[2], "y", 40);

            layout.children[3].Layout.alignment = Qt.AlignLeft|Qt.AlignVCenter
            tryCompare(layout.children[3], "x", 40);
            tryCompare(layout.children[3], "y", 45);

            layout.children[4].Layout.alignment = Qt.AlignLeft
            tryCompare(layout.children[4], "x", 0);
            tryCompare(layout.children[4], "y", 60);

            layout.destroy();
        }


        Component {
            id: layout_rightToLeft_Component
            GridLayout {
                layoutDirection: Qt.RightToLeft
                columnSpacing: 0
                rowSpacing: 0
                columns: 3
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    color: "#cbffc4"
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignCenter
                }
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    color: "#c4d1ff"
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignRight
                }
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    color: "#ffd5c4"
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignLeft
                }
            }
        }

        function verifyIsRightToLeft(layout)
        {
            tryCompare(layout.children[0], "itemRect", [125, 0, 50, 50]);
            tryCompare(layout.children[1], "itemRect", [60,  0, 50, 50]);
            tryCompare(layout.children[2], "itemRect", [10,  0, 50, 50]);
        }

        function verifyIsLeftToRight(layout)
        {
            tryCompare(layout.children[0], "itemRect", [5,   0, 50, 50]);
            tryCompare(layout.children[1], "itemRect", [70,  0, 50, 50]);
            tryCompare(layout.children[2], "itemRect", [120, 0, 50, 50]);
        }

        function test_rightToLeft()
        {
            var layout = layout_rightToLeft_Component.createObject(container);
            layout.width = 180;
            layout.height = 50;

            // Right To Left
            verifyIsRightToLeft(layout)
            layout.LayoutMirroring.enabled = true
            layout.layoutDirection = Qt.LeftToRight
            verifyIsRightToLeft(layout)

            // Left To Right
            layout.LayoutMirroring.enabled = false
            layout.layoutDirection = Qt.LeftToRight
            verifyIsLeftToRight(layout);
            layout.LayoutMirroring.enabled = true
            layout.layoutDirection = Qt.RightToLeft
            verifyIsLeftToRight(layout);

            layout.LayoutMirroring.enabled = false
            verifyIsRightToLeft(layout)

            layout.layoutDirection = Qt.LeftToRight
            verifyIsLeftToRight(layout);

            layout.LayoutMirroring.enabled = true
            verifyIsRightToLeft(layout)

            layout.destroy();
        }

        Component {
            id: layout_columnsOrRowsChanged_Component
            GridLayout {
                id: layout
                rowSpacing: 0
                columnSpacing: 0
                Repeater {
                    model: 4
                    Rectangle {
                        property var itemRect: [x, y, width, height]
                        width: 10
                        height: 10
                        color: "#ff0000"
                    }
                }
            }
        }

        function test_columnsChanged()
        {
            var layout = layout_columnsOrRowsChanged_Component.createObject(container);
            layout.width = 40;
            layout.height = 20;
            tryCompare(layout.children[0], "itemRect", [ 0,  5,  10, 10])
            tryCompare(layout.children[1], "itemRect", [10,  5,  10, 10])
            tryCompare(layout.children[2], "itemRect", [20,  5,  10, 10])
            tryCompare(layout.children[3], "itemRect", [30,  5,  10, 10])

            layout.columns = 2
            tryCompare(layout.children[0], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[1], "itemRect", [20,  0,  10, 10])
            tryCompare(layout.children[2], "itemRect", [ 0, 10,  10, 10])
            tryCompare(layout.children[3], "itemRect", [20, 10,  10, 10])

            layout.destroy()
        }

        function test_rowsChanged()
        {
            var layout = layout_columnsOrRowsChanged_Component.createObject(container);
            layout.flow = GridLayout.TopToBottom
            layout.width = 20;
            layout.height = 40;
            tryCompare(layout.children[0], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0, 10,  10, 10])
            tryCompare(layout.children[2], "itemRect", [ 0, 20,  10, 10])
            tryCompare(layout.children[3], "itemRect", [ 0, 30,  10, 10])

            layout.rows = 2
            tryCompare(layout.children[0], "itemRect", [ 0,  5,  10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0, 25,  10, 10])
            tryCompare(layout.children[2], "itemRect", [10,  5,  10, 10])
            tryCompare(layout.children[3], "itemRect", [10, 25,  10, 10])

            layout.destroy()
        }

        Component {
            id: layout_columnOrRowChanged_Component
            GridLayout {
                id: layout
                rowSpacing: 0
                columnSpacing: 0
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    width: 10
                    height: 10
                    Layout.column: 0
                    color: "#ff0000"
                }
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    Layout.column: 1
                    width: 10
                    height: 10
                    color: "#ff0000"
                }
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    //Layout.column: 2
                    width: 10
                    height: 10
                    color: "#ff0000"
                }
            }
        }

        function test_columnOrRowChanged()
        {
            var layout = layout_columnOrRowChanged_Component.createObject(container);
            layout.width = layout.implicitWidth
            layout.height = layout.implicitHeight
            // c0-c1-c2
            tryCompare(layout.children[0], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[1], "itemRect", [10,  0,  10, 10])
            tryCompare(layout.children[2], "itemRect", [20,  0,  10, 10])

            layout.children[0].Layout.column = 3
            //c1-c2-c0
            tryCompare(layout.children[0], "itemRect", [20,  0,  10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[2], "itemRect", [10,  0,  10, 10])

            layout.children[2].Layout.column = 4
            //c1-c0-c2
            tryCompare(layout.children[0], "itemRect", [10,  0,  10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[2], "itemRect", [20,  0,  10, 10])

            layout.children[0].Layout.row = 1
            // two rows, so we adjust it to its new implicitHeight
            layout.height = layout.implicitHeight
            //c1  c2
            //  c0
            tryCompare(layout.children[0], "itemRect", [10, 10,  10, 10])
            tryCompare(layout.children[1], "itemRect", [ 0,  0,  10, 10])
            tryCompare(layout.children[2], "itemRect", [20,  0,  10, 10])

            layout.destroy()
        }

        Component {
            id: layout_baselines_Component
            GridLayout {
                id: layout
                columnSpacing: 0
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    implicitWidth: 10
                    implicitHeight: 10
                    baselineOffset: 10
                }
                Rectangle {
                    property var itemRect: [x, y, width, height]
                    implicitWidth: 10
                    implicitHeight: 10
                }
            }
        }
        function test_baselines()
        {
            var layout = layout_baselines_Component.createObject(container);
            tryCompare(layout.children[0], "itemRect", [ 0, 0, 10, 10])
            tryCompare(layout.children[1], "itemRect", [10, 0, 10, 10])
            compare(layout.implicitWidth, 20)
            compare(layout.implicitHeight, 10)

            layout.children[0].Layout.alignment = Qt.AlignBaseline
            layout.children[1].Layout.alignment = Qt.AlignBaseline

            tryCompare(layout.children[0], "itemRect", [ 0, 0, 10, 10])
            tryCompare(layout.children[1], "itemRect", [10, 10, 10, 10])
            compare(layout.implicitWidth, 20)
            compare(layout.implicitHeight, 20)

            layout.destroy();
        }

        Component {
            id: layout_spacings_Component
            GridLayout {
                id: layout
                Repeater {
                    model: 2
                    Rectangle {
                        property var itemRect: [x, y, width, height]
                        implicitWidth: 10
                        implicitHeight: 10
                    }
                }
            }
        }

        function test_spacings()
        {
            var layout = layout_spacings_Component.createObject(container);

            // breaks down below -19. This is acceptable, since it means that the implicit size of the layout is negative
            var testSpacings = [Number.NaN, 0, 10, -5, -19]
            layout.rowSpacing = 0
            for (var i = 0; i < testSpacings.length; ++i) {
                var sp = testSpacings[i]
                if (isNaN(sp)) {
                    sp = 5  // Test defaults
                } else {
                    layout.columnSpacing = sp
                }
                tryCompare(layout.children[0], "itemRect", [ 0,  0,  10,  10])
                tryCompare(layout.children[1], "itemRect", [10 + sp,  0,  10,  10])
                compare(layout.implicitWidth, 20 + sp)
            }

            // do not crash
            layout.columnSpacing = -100
            waitForRendering(layout)
            verify(isFinite(layout.implicitWidth))
            layout.destroy();
        }

        Component {
            id: layout_alignToPixelGrid_Component
            GridLayout {
                columns: 3
                rowSpacing: 0
                columnSpacing: 2
                Repeater {
                    model: 3*3
                    Rectangle {
                        color: "red"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
        }

        function test_alignToPixelGrid()
        {
            var layout = layout_alignToPixelGrid_Component.createObject(container)
            layout.width  = 30
            layout.height = 28

            var rectWidth = (layout.width - 2 * layout.columnSpacing)/3
            var rectHeight = layout.height/3

            waitForRendering(layout);

            var sp = layout.columnSpacing
            var idealGeom = [0,0,rectWidth,rectHeight]
            for (var r = 0; r < 3; ++r) {
                idealGeom[0] = 0
                idealGeom[2] = rectWidth
                for (var c = 0; c < 3; ++c) {
                    var child = layout.children[3*r + c]
                    var visualGeom = [child.x, child.y, child.x + child.width, child.y + child.height]

                    // verify that visualGeom is an integer number
                    for (var i = 0; i < 2; ++i)
                        compare(visualGeom[i] % 1, 0)

                    // verify that x,y is no more than one pixel from idealGeom
                    fuzzyCompare(visualGeom[0], idealGeom[0], 1)
                    fuzzyCompare(visualGeom[1], idealGeom[1], 1)

                    // verify that the visual size is no more than 1 pixel taller/wider than the ideal size.
                    verify(visualGeom[2] <= idealGeom[2] + 1)
                    verify(visualGeom[3] <= idealGeom[3] + 1)
                    idealGeom[0] = idealGeom[2] + sp
                    idealGeom[2] = idealGeom[0]  + rectWidth
                }
                idealGeom[1] = idealGeom[3]
                idealGeom[3] = idealGeom[1]  + rectHeight
            }

            layout.destroy()
        }

        Component {

            id: layout_Margins_Component
            GridLayout {
                columns: 2
                rowSpacing: 0
                columnSpacing: 0
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.margins: 10
                    Layout.leftMargin: 2
                    Layout.topMargin: 3
                    Layout.rightMargin: 4
                    Layout.bottomMargin: 4
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.leftMargin: 4
                    Layout.topMargin: 5
                    Layout.rightMargin: 6
                    Layout.bottomMargin: 6
                }
                Rectangle {
                    color: "red"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.leftMargin: 3
                    Layout.topMargin: 4
                    Layout.rightMargin: 5
                    Layout.bottomMargin: 5
                }
            }
        }

        function test_Margins()
        {
            var layout = layout_Margins_Component.createObject(container)

            compare(layout.implicitWidth, 3 + 20 + 5 + 4 + 20 + 6)
            compare(layout.implicitHeight, 5 + 20 + 6 + 4 + 20 + 5)
            layout.width = layout.implicitWidth
            layout.height = layout.implicitHeight

            waitForRendering(layout)

            var c0 = layout.children[0]
            var c1 = layout.children[1]
            var c2 = layout.children[2]

            compare(c0.x, 2)
            compare(c0.y, 5)
            compare(c1.x, 3 + 20 + 5 + 4)
            compare(c1.y, 5)
            compare(c2.x, 3)
            compare(c2.y, 5 + 20 + 6 + 4)

            // reset left|rightMargin. It should then use the generic "margins" property
            c0.Layout.leftMargin = undefined
            compare(layout.implicitWidth, 10 + 20 + 4 + 4 + 20 + 6)
            c0.Layout.bottomMargin = undefined
            compare(layout.implicitHeight, 3 + 20 + 10 + 4 + 20 + 5)
        }

        Component {
            id: layout_invalidateWhileRearranging_Component

            GridLayout {
                columns: 1
                Rectangle {
                    height: 50
                    Layout.fillWidth: true
                    color: 'blue'
                }

                Rectangle {
                    height: 50
                    Layout.fillWidth: true
                    color: 'red'
                    onYChanged: {
                        visible = false;
                    }
                }
            }
        }

        function test_invalidateWhileRearranging_QTBUG_44139()
        {
            var layout = layout_invalidateWhileRearranging_Component.createObject(container)

            waitForRendering(layout);
            verify(layout.children[1].visible == false);
            layout.destroy()
        }
    }
}
