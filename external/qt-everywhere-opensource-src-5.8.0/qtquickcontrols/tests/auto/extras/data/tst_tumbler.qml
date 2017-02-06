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

import QtTest 1.0
import QtQuick 2.1
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Controls.Private 1.0
import QtQuick.Extras 1.4
import "TestUtils.js" as TestUtils

Item {
    id: container
    width: 200
    height: 200

    TestCase {
        id: testCase
        name: "Tests_Tumbler"
        when: windowShown
        anchors.fill: parent

        property var tumbler: null

        property Component simpleColumn: TumblerColumn {
            model: ListModel {
                Component.onCompleted: {
                    for (var i = 0; i < 5; ++i) {
                        append({value: i.toString()});
                    }
                }
            }
        }

        property Component simpleColumn6Items: TumblerColumn {
            model: ListModel {
                Component.onCompleted: {
                    for (var i = 0; i < 6; ++i) {
                        append({value: i.toString()});
                    }
                }
            }
        }

        function init() {
            tumbler = Qt.createQmlObject("import QtQuick.Extras 1.4; Tumbler { }", container, "");
            verify(tumbler, "Tumbler: failed to create an instance");
        }

        function cleanup() {
            tumbler.destroy();
        }

        function test_instance() {
            // Tests instance creation via init() => cleanup().
        }

        function columnXCenter(columnIndex) {
            var columnWidth = tumbler.width / tumbler.columnCount;
            var halfColumnWidth = (columnWidth) / 2;
            return tumbler.__style.padding.left + halfColumnWidth + (columnWidth * columnIndex);
        }

        // visualItemIndex is from 0 to the amount of visible items.
        function itemCenterPos(columnIndex, visualItemIndex) {
            var halfDelegateHeight = tumbler.__style.__delegateHeight / 2;
            var yCenter = tumbler.y + tumbler.__style.padding.top + halfDelegateHeight
                + (tumbler.__style.__delegateHeight * visualItemIndex);
            return Qt.point(columnXCenter(columnIndex), yCenter);
        }

        function test_currentIndex() {
            var column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            compare(tumbler.currentIndexAt(0), 0);
            compare(column.currentIndex, 0);
            waitForRendering(tumbler);

            var pos = Qt.point(columnXCenter(0), tumbler.height / 2);
            mouseDrag(tumbler, pos.x, pos.y, 0, -tumbler.__style.__delegateHeight / 2, Qt.LeftButton, Qt.NoModifier, 200);
            compare(tumbler.currentIndexAt(0), 1);
            compare(column.currentIndex, 1);
        }

        function test_setCurrentIndexAt() {
            var column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            compare(tumbler.currentIndexAt(0), 0);
            waitForRendering(tumbler);

            tumbler.setCurrentIndexAt(0, -1);
            compare(tumbler.currentIndexAt(0), 0);

            tumbler.setCurrentIndexAt(0, -2);
            compare(tumbler.currentIndexAt(0), 0);

            tumbler.setCurrentIndexAt(0, tumbler.getColumn(0).model.count);
            compare(tumbler.currentIndexAt(0), 0);

            tumbler.setCurrentIndexAt(0, tumbler.getColumn(0).model.count + 1);
            compare(tumbler.currentIndexAt(0), 0);

            tumbler.setCurrentIndexAt(-1, 0);
            for (var i = 0; i < tumbler.columnCount; ++i) {
                compare(tumbler.currentIndexAt(i), 0);
            }

            tumbler.setCurrentIndexAt(-1, 1);
            for (i = 0; i < tumbler.columnCount; ++i) {
                compare(tumbler.currentIndexAt(i), 0);
            }

            tumbler.setCurrentIndexAt(0, 1);
            tryCompare(tumbler.__viewAt(0), "offset", 4);
            compare(tumbler.currentIndexAt(0), 1);

            tumbler.setCurrentIndexAt(0, 0);
            waitForRendering(tumbler);
            tumbler.setCurrentIndexAt(0, tumbler.getColumn(0).model.count-1, 1000);
            tryCompare(tumbler.__viewAt(0), "offset", 1);
            compare(tumbler.currentIndexAt(0), tumbler.getColumn(0).model.count-1);
        }

        function test_visible() {
            var column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            compare(tumbler.currentIndexAt(0), 0);
            waitForRendering(tumbler);

            tumbler.getColumn(1).visible = false;
            verify(!tumbler.__viewAt(1).visible);
            // Right-most column never has a separator.
            compare(tumbler.__viewAt(1).parent.separator, null);

            tumbler.getColumn(1).visible = true;
            verify(tumbler.__viewAt(1).visible);

            tumbler.getColumn(0).visible = false;
            verify(!tumbler.__viewAt(0).visible);
            if (Settings.styleName === "Base")
                verify(!tumbler.__viewAt(0).parent.separator.visible);
        }

        function test_keyboardNavigation() {
            if (Qt.platform.os === "osx")
                skip("OS X doesn't allow tab focus for certain controls by default");

            var column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            column = simpleColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            compare(tumbler.currentIndexAt(0), 0);
            waitForRendering(tumbler);

            // Tab through each column twice.
            for (var i = 0; i < 4; ++i) {
                var columnIndex = i % 2;
                // Speed it up.
                tumbler.__highlightMoveDuration = 50;

                keyClick(Qt.Key_Tab);
                verify(tumbler.__viewAt(columnIndex).activeFocus);

                // Navigate upwards through entire column.
                for (var j = 0; j < column.model.count - 1; ++j) {
                    tryCompare(tumbler.__movementDelayTimer, "running", false);
                    keyClick(Qt.Key_Up);
                    tryCompare(tumbler.__viewAt(columnIndex), "offset", j + 1);
                    compare(tumbler.currentIndexAt(columnIndex), column.model.count - 1 - j);
                }

                tryCompare(tumbler.__movementDelayTimer, "running", false);
                keyClick(Qt.Key_Up);
                tryCompare(tumbler.__viewAt(columnIndex), "offset", 0);
                compare(tumbler.currentIndexAt(columnIndex), 0);

                // Navigate downwards through entire column.
                for (j = 0; j < column.model.count - 1; ++j) {
                    tryCompare(tumbler.__movementDelayTimer, "running", false);
                    keyClick(Qt.Key_Down);
                    tryCompare(tumbler.__viewAt(columnIndex), "offset", column.model.count - 1 - j);
                    compare(tumbler.currentIndexAt(columnIndex), j + 1);
                }

                tryCompare(tumbler.__movementDelayTimer, "running", false);
                keyClick(Qt.Key_Down);
                tryCompare(tumbler.__viewAt(columnIndex), "offset", 0);
                compare(tumbler.currentIndexAt(columnIndex), 0);
            }

            // Shift-tab through columns. Focus is on the last column.
            for (i = 0; i < 4; ++i) {
                keyClick(Qt.Key_Tab, Qt.ShiftModifier);
                verify(tumbler.__viewAt(i % 2).activeFocus);
            }

            // Go back to the first column.
            keyClick(Qt.Key_Tab, Qt.ShiftModifier);
            verify(tumbler.__viewAt(0).activeFocus);
            compare(tumbler.__viewAt(0).offset, 0);
        }

        property Component fourItemColumn: TumblerColumn {
            model: 4
        }

        property Component fourItemDelegate: Item {
            implicitHeight: 40

            Text {
                text: styleData.value
                anchors.centerIn: parent
            }
        }

        function test_itemsCorrectlyPositioned() {
            if (Qt.platform.os === "osx")
                skip("OS X doesn't allow tab focus for certain controls by default");

            // TODO: rewrite this test so that it tests supported usecases.
            // Somehow it works with the Base style. It could be rewritten to use an
            // equal amount of items for the model and visibleItemCount, judging from
            // the snippet in QTBUG-40298.
            if (Settings.styleName === "Flat")
                skip("Not a valid test case as the model count is less than the visibleItemCount");

            tumbler.height = 120;
            // By default, the delegate height is based on the height of the tumbler,
            // but it starts off at 0.
            compare(tumbler.__style.__delegateHeight, 0);

            var column = fourItemColumn.createObject(tumbler);
            column.delegate = fourItemDelegate;
            compare(tumbler.addColumn(column), column);
            // Now that the delegate has changed, the binding is reevaluated and we get 120 / 3.
            compare(tumbler.__style.__delegateHeight, 40);
            waitForRendering(tumbler);

            keyClick(Qt.Key_Tab)
            verify(tumbler.__viewAt(0).activeFocus);
            var firstItemCenterPos = itemCenterPos(0, 1);
            var firstItem = tumbler.__viewAt(0).itemAt(firstItemCenterPos.x, firstItemCenterPos.y);
            var actualPos = container.mapFromItem(firstItem, 0, 0);
            compare(actualPos.x, tumbler.__style.padding.left);
            compare(actualPos.y, tumbler.__style.padding.top + 40);

            keyClick(Qt.Key_Down);
            tryCompare(tumbler.__viewAt(0), "offset", 3.0);
            firstItemCenterPos = itemCenterPos(0, 0);
            firstItem = tumbler.__viewAt(0).itemAt(firstItemCenterPos.x, firstItemCenterPos.y);
            verify(firstItem);
            // Test QTBUG-40298.
            actualPos = container.mapFromItem(firstItem, 0, 0);
            compare(actualPos.x, tumbler.__style.padding.left);
            compare(actualPos.y, tumbler.__style.padding.top);

            var secondItemCenterPos = itemCenterPos(0, 1);
            var secondItem = tumbler.__viewAt(0).itemAt(secondItemCenterPos.x, secondItemCenterPos.y);
            verify(secondItem);
            verify(firstItem.y < secondItem.y);

            var thirdItemCenterPos = itemCenterPos(0, 2);
            var thirdItem = tumbler.__viewAt(0).itemAt(thirdItemCenterPos.x, thirdItemCenterPos.y);
            verify(thirdItem);
            verify(firstItem.y < thirdItem.y);
            verify(secondItem.y < thirdItem.y);
        }

        property Component oneHundredItemColumn: TumblerColumn {
            model: ListModel {
                Component.onCompleted: {
                    for (var i = 0; i < 100; ++i) {
                        append({value: i.toString()});
                    }
                }
            }
        }

        function test_resizeAfterFlicking() {
            // Test QTBUG-40367 (which is actually invalid because it was my fault :)).
            var column = oneHundredItemColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            waitForRendering(tumbler);

            // Flick in some direction.
            var pos = Qt.point(columnXCenter(0), tumbler.__style.padding.top);
            mouseDrag(tumbler, pos.x, pos.y, 0, tumbler.height - tumbler.__style.padding.bottom,
                Qt.LeftButton, Qt.NoModifier, 300);
            tryCompare(tumbler.__viewAt(0), "offset", Settings.styleName === "Flat" ? 6.0 : 4.0);

            tumbler.height += 100;
            var padding = tumbler.__style.padding;
            compare(tumbler.__style.__delegateHeight,
                (tumbler.height - padding.top - padding.bottom) / tumbler.__style.visibleItemCount);
            waitForRendering(tumbler);
            pos = itemCenterPos(0, 1);
            var ninetyEighthItem = tumbler.__viewAt(0).itemAt(pos.x, pos.y);
            verify(ninetyEighthItem);
        }

        property Component dayOfMonthColumn: TumblerColumn {
            model: ListModel {
                Component.onCompleted: {
                    for (var i = 0; i < 31; ++i) {
                        append({value: i.toString()});
                    }
                }
            }
        }

        property Component yearColumn: TumblerColumn {
            model: ListModel {
                Component.onCompleted: {
                    for (var i = 2000; i < 2100; ++i) {
                        append({value: i.toString()});
                    }
                }
            }
        }

        function test_focusPastLastColumn() {
            if (Qt.platform.os === "osx")
                skip("OS X doesn't allow tab focus for certain controls by default");

            var column = dayOfMonthColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            column = yearColumn.createObject(tumbler);
            compare(tumbler.addColumn(column), column);

            var mouseArea = Qt.createQmlObject(
                "import QtQuick 2.2; MouseArea { activeFocusOnTab: true; width: 50; height: 50 }", container, "");

            keyClick(Qt.Key_Tab);
            verify(tumbler.__viewAt(0).activeFocus);
            verify(tumbler.getColumn(0).activeFocus);
            verify(!tumbler.__viewAt(1).activeFocus);
            verify(!tumbler.getColumn(1).activeFocus);

            keyClick(Qt.Key_Tab);
            verify(!tumbler.__viewAt(0).activeFocus);
            verify(!tumbler.getColumn(0).activeFocus);
            verify(tumbler.__viewAt(1).activeFocus);
            verify(tumbler.getColumn(1).activeFocus);

            keyClick(Qt.Key_Tab);
            verify(!tumbler.__viewAt(0).activeFocus);
            verify(!tumbler.getColumn(0).activeFocus);
            verify(!tumbler.__viewAt(1).activeFocus);
            verify(!tumbler.getColumn(1).activeFocus);
            verify(mouseArea.activeFocus);

            mouseArea.destroy();
        }

        function test_datePicker() {
            tumbler.destroy();

            var component = Qt.createComponent("TumblerDatePicker.qml");
            compare(component.status, Component.Ready);
            tumbler = component.createObject(container);
            // Should not be any warnings.

            // March.
            tumbler.setCurrentIndexAt(1, 2);
            compare(tumbler.currentIndexAt(1), 2);
            compare(tumbler.getColumn(1).currentIndex, 2);

            // 30th of March.
            tumbler.setCurrentIndexAt(0, 29);
            compare(tumbler.currentIndexAt(0), 29);
            compare(tumbler.getColumn(0).currentIndex, 29);

            // February.
            tumbler.setCurrentIndexAt(1, 1);
            compare(tumbler.currentIndexAt(1), 1);
            compare(tumbler.getColumn(1).currentIndex, 1);
            compare(tumbler.getColumn(0).currentIndex, 27);
        }

        property Component displacementStyle: TumblerStyle {
            visibleItemCount: 5

            delegate: Item {
                objectName: "delegate" + styleData.index
                implicitHeight: (control.height - padding.top - padding.bottom) / visibleItemCount

                property real displacement: styleData.displacement

                Text {
                    text: styleData.value
                    anchors.centerIn: parent
                }

                Text {
                    anchors.right: parent.right
                    text: styleData.displacement.toFixed(1)
                }
            }
        }

        function test_displacement_data() {
            var data = [
                // At 0 offset, the first item is current.
                { index: 0, offset: 0, expectedDisplacement: 0 },
                { index: 1, offset: 0, expectedDisplacement: -1 },
                { index: 5, offset: 0, expectedDisplacement: 1 },
                // When we start to move the first item down, the second item above it starts to become current.
                { index: 0, offset: 0.25, expectedDisplacement: -0.25 },
                { index: 1, offset: 0.25, expectedDisplacement: -1.25 },
                { index: 5, offset: 0.25, expectedDisplacement: 0.75 },
                { index: 0, offset: 0.5, expectedDisplacement: -0.5 },
                { index: 1, offset: 0.5, expectedDisplacement: -1.5 },
                { index: 5, offset: 0.5, expectedDisplacement: 0.5 },
                // By this stage, the delegate at index 1 is destroyed, so we can't test its displacement.
                { index: 0, offset: 0.75, expectedDisplacement: -0.75 },
                { index: 5, offset: 0.75, expectedDisplacement: 0.25 },
                { index: 0, offset: 4.75, expectedDisplacement: 1.25 },
                { index: 1, offset: 4.75, expectedDisplacement: 0.25 },
                { index: 0, offset: 4.5, expectedDisplacement: 1.5 },
                { index: 1, offset: 4.5, expectedDisplacement: 0.5 },
                { index: 0, offset: 4.25, expectedDisplacement: 1.75 },
                { index: 1, offset: 4.25, expectedDisplacement: 0.75 }
            ];
            for (var i = 0; i < data.length; ++i) {
                var row = data[i];
                row.tag = "delegate" + row.index + " offset=" + row.offset + " expectedDisplacement=" + row.expectedDisplacement;
            }
            return data;
        }

        function test_displacement(data) {
            tumbler.style = displacementStyle;

            var column = simpleColumn6Items.createObject(tumbler);
            compare(tumbler.addColumn(column), column);
            waitForRendering(tumbler);
            compare(tumbler.columnCount, 1);
            compare(tumbler.__viewAt(0).count, 6);

            var delegate = TestUtils.findChild(tumbler, "delegate" + data.index);
            verify(delegate);

            tumbler.__viewAt(0).offset = data.offset;
            compare(delegate.displacement, data.expectedDisplacement);
        }

        function test_visibleItemCount_data() {
            var data = [
                // e.g. {0: 2} = {delegate index: y pos / delegate height}
                // Skip item at index 3, because it's out of view.
                { model: 6, visibleItemCount: 5, expectedYPositions: {0: 2, 1: 3, 2: 4, 4: 0} },
                { model: 5, visibleItemCount: 3, expectedYPositions: {0: 1, 1: 2, 4: 0} },
                // Takes up the whole view.
                { model: 2, visibleItemCount: 1, expectedYPositions: {0: 0} },
            ];

            for (var i = 0; i < data.length; ++i) {
                data[i].tag = "items=" + data[i].model + ", visibleItemCount=" + data[i].visibleItemCount;
            }
            return data;
        }

        function test_visibleItemCount(data) {
            tumbler.style = displacementStyle;
            tumbler.__style.visibleItemCount = data.visibleItemCount;

            var column = simpleColumn.createObject(tumbler);
            column.model = data.model;
            compare(tumbler.addColumn(column), column);
            waitForRendering(tumbler);
            compare(tumbler.columnCount, 1);
            compare(tumbler.__viewAt(0).count, data.model);

            for (var delegateIndex = 0; delegateIndex < data.visibleItemCount; ++delegateIndex) {
                if (data.expectedYPositions.hasOwnProperty(delegateIndex)) {
                    var delegate = TestUtils.findChild(tumbler, "delegate" + delegateIndex);
                    verify(delegate, "Delegate found at index " + delegateIndex);
                    var expectedYPos = data.expectedYPositions[delegateIndex] * tumbler.__style.__delegateHeight;
                    compare(delegate.mapToItem(tumbler.__viewAt(0), 0, 0).y, expectedYPos);
                }
            }
        }
    }
}
