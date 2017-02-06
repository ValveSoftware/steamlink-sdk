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
import QtQuick.Controls 1.1
import QtQuick.Controls.Private 1.0
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0
import QtQuick.Extras.Private.CppUtils 1.0

// Have to have Item here otherwise mouse clicks don't work.
Item {
    id: container
    width: 400
    height: 400

    TestCase {
        id: testcase
        name: "Tests_PieMenu"
        when: windowShown
        anchors.fill: parent

        readonly property real menuWidth: 200
        readonly property real menuHeight: 200

        // The root item for each test, if one exists.
        property Item root
        // The pie menu for each test, if no root is created.
        property Item pieMenu

        SignalSpy {
            id: currentIndexSignalSpy
        }

        SignalSpy {
            id: actionSignalSpy
        }

        SignalSpy {
            id: selectedAngleChangedSpy
        }

        function cleanup() {
            currentIndexSignalSpy.clear();
            actionSignalSpy.clear();
            selectedAngleChangedSpy.clear();

            if (root)
                root.destroy();
            if (pieMenu)
                pieMenu.destroy();
        }

        function mouseButtonToString(button) {
            return button === Qt.LeftButton ? "Qt.LeftButton" : "Qt.RightButton";
        }

        function triggerModeToString(triggerMode) {
            return triggerMode === TriggerMode.TriggerOnPress ? "TriggerOnPress"
                : (triggerMode === TriggerMode.TriggerOnRelease ? "TriggerOnRelease" : "TriggerOnClick");
        }

        function test_instance() {
            var pieMenu = Qt.createQmlObject("import QtQuick.Extras 1.4; PieMenu { }", container, "");
            verify(pieMenu, "PieMenu: failed to create an instance");
            verify(pieMenu.__style);
            compare(pieMenu.triggerMode, TriggerMode.TriggerOnClick);
            pieMenu.destroy();

            // Ensure setting visible = true; visible = false; in onCompleted doesn't cause any problems.
            var pieMenuComponent = Qt.createComponent("PieMenuVisibleOnCompleted.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            pieMenu = pieMenuComponent.createObject(container);
            verify(pieMenu, "PieMenu: failed to create an instance");
            pieMenu.destroy();

            // Ensure constructing a menu as a property (and hence no parent)
            // with visible = true doesn't cause any problems.
            pieMenuComponent = Qt.createComponent("PieMenuVisibleButNoParent.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            pieMenu = pieMenuComponent.createObject(container);
            verify(pieMenu, "PieMenu: failed to create an instance");
            pieMenu.destroy();
        }

        function test_triggerMode() {
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;
            currentIndexSignalSpy.signalName = "currentIndexChanged"
            currentIndexSignalSpy.target = pieMenu;
            actionSignalSpy.signalName = "actionTriggered";
            actionSignalSpy.target = root;

            compare(pieMenu.triggerMode, TriggerMode.TriggerOnClick);

            var triggerModes = [
                TriggerMode.TriggerOnClick,
                TriggerMode.TriggerOnPress,
                TriggerMode.TriggerOnRelease
            ];
            // Our root mouse area will accept either left click or right click,
            // and the menu should accept either to select items or close it.
            var buttonVariations = [
                [Qt.LeftButton, Qt.LeftButton],
                [Qt.RightButton, Qt.RightButton],
                [Qt.LeftButton, Qt.RightButton],
                [Qt.RightButton, Qt.LeftButton]
            ];
            for (var modeIndex = 0; modeIndex < triggerModes.length; ++modeIndex) {
                for (var i = 0; i < buttonVariations.length; ++i) {
                    var openButton = buttonVariations[i][0];
                    var closeButton = buttonVariations[i][1];

                    pieMenu.triggerMode = triggerModes[modeIndex];
                    compare(pieMenu.triggerMode, triggerModes[modeIndex]);

                    // Make the menu visible.
                    if (pieMenu.triggerMode !== TriggerMode.TriggerOnRelease) {
                        mouseClick(root, 0, 0, openButton);
                    } else {
                        mousePress(root, 0, 0, openButton);
                    }
                    tryCompare(pieMenu, "visible", true);

                    // Click/press outside the menu to close it.
                    switch (pieMenu.triggerMode) {
                    case TriggerMode.TriggerOnClick:
                        mouseClick(root, 0, 0, closeButton);
                        tryCompare(currentIndexSignalSpy, "count", 0);
                        compare(pieMenu.visible, false, "Menu isn't closed when clicking "
                            + mouseButtonToString(closeButton) + " outside (triggerMode: "
                            + triggerModeToString(pieMenu.triggerMode) + ", openButton: "
                            + mouseButtonToString(openButton) + ")");
                        break;
                    case TriggerMode.TriggerOnPress:
                        mousePress(root, 0, 0, closeButton);
                        tryCompare(currentIndexSignalSpy, "count", 0);
                        compare(pieMenu.visible, false, "Menu isn't closed when pressing "
                            + mouseButtonToString(closeButton) + " outside (triggerMode: "
                            + triggerModeToString(pieMenu.triggerMode) + ", openButton: "
                            + mouseButtonToString(openButton) + ")");
                        mouseRelease(root, 0, 0, closeButton);
                        tryCompare(currentIndexSignalSpy, "count", 0);
                        compare(pieMenu.visible, false, "Menu shouldn't be opened when releasing "
                            + mouseButtonToString(closeButton) + " outside (triggerMode: "
                            + triggerModeToString(pieMenu.triggerMode) + ", openButton: "
                            + mouseButtonToString(openButton) + ")");
                        break;
                    case TriggerMode.TriggerOnRelease:
                        mouseRelease(root, 0, 0, closeButton);
                        tryCompare(currentIndexSignalSpy, "count", 0);
                        compare(pieMenu.visible, false, "Menu is closed when releasing "
                            + mouseButtonToString(closeButton) + " outside");
                        break;
                    }

                    // Make the menu visible again.
                    if (pieMenu.triggerMode !== TriggerMode.TriggerOnRelease) {
                        mouseClick(root, 0, 0, openButton);
                    } else {
                        mousePress(root, 0, 0, openButton);
                    }
                    tryCompare(pieMenu, "visible", true);

                    switch (pieMenu.triggerMode) {
                    case TriggerMode.TriggerOnClick:
                        // Click on a section of the menu (index 1); it should trigger the item and then close the menu.
                        mouseClick(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 4, closeButton);
                        // The current index changes once when the item is selected (1) and once when the menu closes (-1).
                        tryCompare(currentIndexSignalSpy, "count", 2);
                        compare(actionSignalSpy.count, 1);
                        compare(actionSignalSpy.signalArguments[0][0], 1);
                        compare(pieMenu.currentIndex, -1);
                        compare(pieMenu.visible, false);
                        break;
                    case TriggerMode.TriggerOnPress:
                        // Press on a section of the menu (index 1); it should trigger the item and then close the menu.
                        mousePress(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 4, closeButton);
                        // The current index changes once when the item is selected (1) and once when the menu closes (-1).
                        tryCompare(currentIndexSignalSpy, "count", 2);
                        compare(actionSignalSpy.count, 1);
                        compare(actionSignalSpy.signalArguments[0][0], 1);
                        compare(pieMenu.currentIndex, -1);
                        compare(pieMenu.visible, false);

                        mouseRelease(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 4, closeButton);
                        // None of these should change after releasing.
                        tryCompare(currentIndexSignalSpy, "count", 2);
                        compare(actionSignalSpy.count, 1);
                        compare(pieMenu.currentIndex, -1);
                        compare(pieMenu.visible, false);
                        break;
                    case TriggerMode.TriggerOnRelease:
                        // Click/press on a section of the menu (index 1); it should trigger the item and then close the menu.
                        mouseRelease(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 4, closeButton);
                        // The current index changes once when the item is selected (1) and once when the menu closes (-1).
                        tryCompare(currentIndexSignalSpy, "count", 2);
                        compare(actionSignalSpy.count, 1);
                        compare(actionSignalSpy.signalArguments[0][0], 1);
                        compare(pieMenu.currentIndex, -1);
                        compare(pieMenu.visible, false);
                    }

                    currentIndexSignalSpy.clear();
                    actionSignalSpy.clear();
                }
            }

            pieMenuComponent.destroy()
        }

        function test_selectionAngle_data() {
            var data = [];

            var dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = 90;
            dataRow.mouseX = 1;
            dataRow.mouseY = 1;
            dataRow.expectedAngle = -Math.PI / 4;
            dataRow.expectedCurrentIndex = -1;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", top left";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = 90;
            dataRow.mouseX = menuWidth - 1;
            dataRow.mouseY = menuHeight - 1;
            dataRow.expectedAngle = Math.PI * 0.75;
            dataRow.expectedCurrentIndex = -1;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom right";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = -270;
            dataRow.mouseX = menuWidth / 2 - 1;
            dataRow.mouseY = menuHeight * 0.75;
            dataRow.expectedAngle = -3.1215953196166426;
            dataRow.expectedCurrentIndex = 1;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom left edge";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = -270;
            dataRow.mouseX = menuWidth / 2 + 1;
            dataRow.mouseY = menuHeight * 0.75;
            dataRow.expectedAngle = 3.1215953196166426;
            dataRow.expectedCurrentIndex = 1;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom right edge";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = 0;
            dataRow.endAngle = 190;
            dataRow.mouseX = menuWidth / 2 - 1;
            dataRow.mouseY = menuHeight * 0.75;
            dataRow.expectedAngle = -3.1215953196166426;
            dataRow.expectedCurrentIndex = 2;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom left edge";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = 0;
            dataRow.endAngle = -90;
            dataRow.mouseX = menuWidth / 4;
            dataRow.mouseY = menuHeight / 2 - 1;
            dataRow.expectedAngle = -1.550798992821746;
            dataRow.expectedCurrentIndex = 2;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", section 2";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = 0;
            dataRow.endAngle = -90;
            dataRow.mouseX = menuWidth / 4;
            dataRow.mouseY = menuHeight / 4;
            dataRow.expectedAngle = -0.7853981633974483;
            dataRow.expectedCurrentIndex = 1;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", section 1";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = 0;
            dataRow.endAngle = -90;
            dataRow.mouseX = menuWidth / 2 - 1;
            dataRow.mouseY = menuHeight / 4;
            dataRow.expectedAngle = -0.01999733397315053;
            dataRow.expectedCurrentIndex = 0;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", section 0";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = -190;
            dataRow.endAngle = -90;
            dataRow.mouseX = menuWidth / 2 + 1;
            dataRow.mouseY = menuHeight * 0.75;
            dataRow.expectedAngle = 3.1215953196166426;
            dataRow.expectedCurrentIndex = 0;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom right edge";
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = -190;
            dataRow.mouseX = menuWidth / 2 + 1;
            dataRow.mouseY = menuHeight * 0.75;
            dataRow.expectedAngle = 3.1215953196166426;
            dataRow.expectedCurrentIndex = 2;
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", bottom right edge";
            data.push(dataRow);

            return data;
        }

        function test_selectionAngle(data) {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            compare(pieMenu.selectionAngle, 0);

            pieMenu.__style.startAngle = data.startAngle;
            pieMenu.__style.endAngle = data.endAngle;

            waitForRendering(root);
            root.forceActiveFocus();
            // Don't allow bounds snapping by always opening within bounds.
            mouseClick(root, menuWidth / 2, menuHeight / 2);
            tryCompare(pieMenu, "visible", true);

            mouseMove(root, data.mouseX, data.mouseY);
            compare(pieMenu.selectionAngle, data.expectedAngle);
            compare(pieMenu.currentIndex, data.expectedCurrentIndex);

            pieMenuComponent.destroy()
        }

        function test_sectionAngles_data() {
            var data = [];
            var angleOrigin = 90;

            var dataRow = {};
            dataRow.startAngle = -90;
            dataRow.endAngle = 90;
            dataRow.section = 0;
            dataRow.expectedSectionSize = MathUtils.degToRadOffset(60 + angleOrigin);
            dataRow.expectedSectionStartAngle = MathUtils.degToRadOffset(-90);
            dataRow.expectedSectionCenterAngle = MathUtils.degToRadOffset(-60);
            dataRow.expectedSectionEndAngle = MathUtils.degToRadOffset(-30);
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", section=" + dataRow.section;
            data.push(dataRow);

            dataRow = {};
            dataRow.startAngle = 270;
            dataRow.endAngle = 90;
            dataRow.section = 0;
            dataRow.expectedSectionSize = MathUtils.degToRadOffset(-60 + angleOrigin);
            dataRow.expectedSectionStartAngle = MathUtils.degToRadOffset(270);
            dataRow.expectedSectionCenterAngle = MathUtils.degToRadOffset(240);
            dataRow.expectedSectionEndAngle = MathUtils.degToRadOffset(210);
            dataRow.tag = "startAngle=" + dataRow.startAngle + ", endAngle=" + dataRow.endAngle + ", section=" + dataRow.section;
            data.push(dataRow);

            return data;
        }

        function test_sectionAngles(data) {
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var pieMenu = root.pieMenu;

            pieMenu.__style.startAngle = data.startAngle;
            pieMenu.__style.endAngle = data.endAngle;
            compare(pieMenu.__protectedScope.sectionSize, data.expectedSectionSize);
            compare(pieMenu.__protectedScope.sectionStartAngle(data.section), data.expectedSectionStartAngle);
            compare(pieMenu.__protectedScope.sectionCenterAngle(data.section), data.expectedSectionCenterAngle);
            compare(pieMenu.__protectedScope.sectionEndAngle(data.section), data.expectedSectionEndAngle);

            pieMenuComponent.destroy()
        }

        function test_bounds_data() {
            return [
                { tag: "noSnapCenter", mouseX: container.width / 2, mouseY: container.height / 2,
                    expectedX: container.width / 2 - 100, expectedY: container.height / 2 - 100 },
                { tag: "noSnapNearLeft", mouseX: 100, mouseY: container.height / 2,
                    expectedX: 0, expectedY: container.height / 2 - 100 },
                { tag: "noSnapNearRight", mouseX: container.width - 100, mouseY: container.height / 2,
                    expectedX: container.width - menuWidth, expectedY: container.height / 2 - 100 },
                { tag: "noSnapNearTop", mouseX: container.width / 2, mouseY: 100,
                    expectedX: container.width / 2 - 100, expectedY: 0 },
                { tag: "noSnapNearBottom", mouseX: container.width / 2, mouseY: container.height - 100,
                    expectedX: container.width / 2 - 100, expectedY: container.height - menuHeight },
                { tag: "noSnapNearTopLeft", mouseX: 100, mouseY: 100,
                    expectedX: 0, expectedY: 0 },
                { tag: "noSnapNearTopRight", mouseX: container.width - 100, mouseY: 100,
                    expectedX: container.width - menuWidth, expectedY: 0 },
                { tag: "noSnapNearBottomRight", mouseX: container.width - 100, mouseY: container.height - 100,
                    expectedX: container.width - menuWidth, expectedY: container.height - menuHeight },
                { tag: "noSnapNearBottomLeft", mouseX: 100, mouseY: container.height - 100,
                    expectedX: 0, expectedY: container.height - menuHeight },
                { tag: "leftEdge", mouseX: 10, mouseY: container.height / 2,
                    expectedX: 0, expectedY: container.height / 2 - 100 },
                { tag: "rightEdge", mouseX: container.width - 10, mouseY: container.height / 2,
                    expectedX: container.width - menuHeight, expectedY: container.height / 2 - 100 },
                { tag: "topEdge", mouseX: container.width / 2, mouseY: 10,
                    expectedX: container.width / 2 - 100, expectedY: 0 },
                // The default start and end angles mean that the bottom edge won't snap.
                { tag: "bottomEdge", mouseX: container.width / 2, mouseY: container.height - 10,
                    expectedX: container.width / 2 - 100, expectedY: container.height - 100 - 10 },
                { tag: "topLeftCorner", mouseX: 10, mouseY: 10,
                    expectedX: 0, expectedY: 0 },
                { tag: "topRightCorner", mouseX: container.width - 10, mouseY: 10,
                    expectedX: container.width - menuHeight, expectedY: 0 },
                { tag: "bottomRightCorner", mouseX: container.width - 10, mouseY: container.height - 10,
                    expectedX: container.width - menuHeight, expectedY: container.height - 100 - 10 },
                { tag: "bottomLeftCorner", mouseX: 10, mouseY: container.height - 10,
                    expectedX: 0, expectedY: container.height - 100 - 10 }
            ]
        }

        function test_bounds(data) {
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            var rootParent = pieMenu;
            while (rootParent.parent) {
                rootParent = rootParent.parent;
            }
            // Necessary due to QTBUG-36938
            rootParent.width = 400;
            rootParent.height = 400;

            var originalStartAngle = pieMenu.__style.startAngle;
            var originalEndAngle = pieMenu.__style.endAngle;

            mouseClick(root, data.mouseX, data.mouseY);
            tryCompare(pieMenu, "visible", true);

            compare(pieMenu.x, data.expectedX);
            compare(pieMenu.y, data.expectedY);

            // Angles shouldn't change.
            compare(pieMenu.__style.startAngle, originalStartAngle);
            compare(pieMenu.__style.endAngle, originalEndAngle);

            // Cancel the menu. Even if it's at the top left, this will land on the cancel area.
            mouseClick(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 2);
            compare(pieMenu.visible, false);

            // Angles shouldn't change.
            compare(pieMenu.__style.startAngle, originalStartAngle);
            compare(pieMenu.__style.endAngle, originalEndAngle);

            pieMenuComponent.destroy()
        }

        function test_hideItem_data() {
            return [
                { tag: "hideFirst", indexVisibility: [false, true, true], expectedIndexHits: [0, 1, 1], expectedText: ["item1", "item1", "item2"] },
                { tag: "hideSecond", indexVisibility: [true, false, true], expectedIndexHits: [0, 1, 1], expectedText: ["item0", "item0", "item2"] },
                { tag: "hideThird", indexVisibility: [true, true, false], expectedIndexHits: [0, 1, 1], expectedText: ["item0", "item0", "item1"] },
            ];
        }

        function test_hideItem(data) {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            var originalStartAngle = pieMenu.__style.startAngle;
            var originalEndAngle = pieMenu.__style.endAngle;

            // Store the positions at which we should click before we remove any items.
            var mousePositions = [Qt.point(40, 70), Qt.point(90, 30), Qt.point(160, 70)];

            while (pieMenu.menuItems.length > 0) {
                pieMenu.removeItem(pieMenu.menuItems[pieMenu.menuItems.length - 1]);
            }

            for (var i = 0; i < data.indexVisibility.length; ++i) {
                var item = pieMenu.addItem("item" + i);
                item.triggered.connect(function (){ root.actionTriggered(data.expectedIndexHits[i]) })
                item.visible = data.indexVisibility[i];
            }

            // Angles shouldn't change.
            compare(pieMenu.__style.startAngle, originalStartAngle);
            compare(pieMenu.__style.endAngle, originalEndAngle);

            for (i = 0; i < data.indexVisibility.length; ++i) {
                actionSignalSpy.signalName = "actionTriggered";
                actionSignalSpy.target = root;

                compare(pieMenu.visible, false);

                // Make the menu visible.
                mouseClick(root, 100, 100, Qt.LeftButton);
                tryCompare(pieMenu, "visible", true);

                var pos = mousePositions[i];

                mouseMove(root, pos.x, pos.y);
                // Not all styles have titles.
                if (pieMenu.__style.title)
                    tryCompare(pieMenu.__panel.titleItem, "text", data.expectedText[i]);

                mouseClick(root, pos.x, pos.y, Qt.LeftButton);

                tryCompare(pieMenu, "visible", false);
                compare(actionSignalSpy.count, 1);
                compare(actionSignalSpy.signalArguments[0][0], data.expectedIndexHits[i]);

                actionSignalSpy.clear();
            }

            pieMenuComponent.destroy()
        }

        function test_addItem() {
            pieMenu = Qt.createQmlObject("import QtQuick.Extras 1.1; PieMenu {}", container);
            compare(pieMenu.menuItems.length, 0);

            pieMenu.addItem("Action 1");
            compare(pieMenu.menuItems.length, 1);
        }

        function test_insertItem() {
            pieMenu = Qt.createQmlObject("import QtQuick.Extras 1.1; PieMenu {}", container);
            compare(pieMenu.menuItems.length, 0);

            pieMenu.insertItem(0, "Action 1");
            compare(pieMenu.menuItems.length, 1);
            compare(pieMenu.menuItems[0].text, "Action 1");

            pieMenu.insertItem(1, "Action 2");
            compare(pieMenu.menuItems.length, 2);
            compare(pieMenu.menuItems[0].text, "Action 1");
            compare(pieMenu.menuItems[1].text, "Action 2");
        }

        function test_removeItem() {
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var pieMenu = root.pieMenu;

            var originalLength = pieMenu.menuItems.length;
            for (var i = 0; i < originalLength; ++i) {
                pieMenu.removeItem(pieMenu.menuItems[pieMenu.menuItems.length - 1]);
                compare(pieMenu.menuItems.length, originalLength - (i + 1));
            }

            pieMenuComponent.destroy()
        }

        function debugMousePosition(pieMenu, mouseX, mouseY, positionText) {
            var rectItem = Qt.createQmlObject("import QtQuick 2.0; Rectangle { width: 10; height: 10; radius: 5; color: 'red' }", pieMenu);
            rectItem.x = mouseX - rectItem.width / 2;
            rectItem.y = mouseY - rectItem.height / 2;

            var textItem = Qt.createQmlObject("import QtQuick 2.0; Text {}", rectItem);
            textItem.text = positionText;
            textItem.font.pixelSize = 8;
            textItem.anchors.centerIn = textItem.parent;
        }

        function test_selectionItemOnMouseMove_QTRD3024() {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            // Check when an item is hovered by the mouse, it gets made current
            // as expected and the current item is cleared when the mouse moves outside the menu
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            pieMenu = root.pieMenu;

            // Make the menu visible at (0,0)
            waitForRendering(root);
            root.forceActiveFocus();
            mouseClick(root, 0, 0);
            tryCompare(pieMenu, "visible", true);

            // Move the mouse over the menu
            mouseMove(root, 0, 0);
            compare(pieMenu.currentIndex, -1);

            // We would use a data function for this, but we need access to properties
            // of the style that change with each style.
            var data = [];
            var xCenter = pieMenu.width / 2;
            var yCenter = pieMenu.height / 2;
            var startAngleFunction = pieMenu.__protectedScope.sectionStartAngle;
            var centerAngleFunction = pieMenu.__protectedScope.sectionCenterAngle;
            var endAngleFunction = pieMenu.__protectedScope.sectionEndAngle;

            var pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, centerAngleFunction(0), 90)
            data.push({ name: "inside", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 0 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, startAngleFunction(0) + MathUtils.degToRad(1), pieMenu.__style.cancelRadius + 10);
            data.push({ name: "bottom", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 0 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, centerAngleFunction(0), pieMenu.__style.cancelRadius + 10);
            data.push({ name: "outside", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 0 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, endAngleFunction(0) - MathUtils.degToRad(1), pieMenu.__style.radius - 1);
            data.push({ name: "close to second", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 0 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, centerAngleFunction(1), pieMenu.__style.radius - 1);
            data.push({ name: "outside", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 1 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, centerAngleFunction(1) - MathUtils.degToRad(10),
                pieMenu.__style.cancelRadius + (pieMenu.__style.radius - pieMenu.__style.cancelRadius) / 2);
            data.push({ name: "inside", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 1 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, startAngleFunction(1) + MathUtils.degToRad(1),
                pieMenu.__style.radius - 1);
            data.push({ name: "close to first", mouseX: pos.x, mouseY: pos.y, expectedCurrentIndex: 1 });

            pos = MathUtils.centerAlongCircle(xCenter, yCenter, 0, 0, centerAngleFunction(1), pieMenu.__style.cancelRadius + 10);
            data.push({ name: "low center", mouseX: 100, mouseY: 50, expectedCurrentIndex: 1 });

            for (var i = 0; i < data.length; ++i) {
                var mouseX = data[i].mouseX;
                var mouseY = data[i].mouseY;
                var expectedCurrentIndex = data[i].expectedCurrentIndex;

                // Illustrates the positions.
//                debugMousePosition(pieMenu, mouseX, mouseY, i);
//                wait(1000)

                mouseMove(root, mouseX, mouseY);
                compare(pieMenu.currentIndex, expectedCurrentIndex, data[i].name + ": current index should be "
                    + expectedCurrentIndex + " when mouse is at " + mouseX + ", " + mouseY);
            }

            pieMenuComponent.destroy()
        }

        function test_QTRD3027() {
            // Check that an item's selection is cleared when the mouse moves outside
            // its boundaries without changing the selectionAngle
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            // Make the menu visible at (0,0)
            root.forceActiveFocus();
            mouseClick(root, 0, 0);
            tryCompare(pieMenu, "visible", true);

            // Move the mouse over the menu
            mouseMove(root, 0, 0)
            compare(pieMenu.currentIndex, -1)
            // Move over middle item
            mouseMove(root, 100, 50)
            compare(pieMenu.currentIndex, 1)
            selectedAngleChangedSpy.signalName = "selectionAngleChanged"
            selectedAngleChangedSpy.target = pieMenu;
            // Move outside the middle item without changing angle
            mouseMove(root, 100, 98)
            compare(pieMenu.currentIndex, -1)
            compare(selectedAngleChangedSpy.count, 0)

            pieMenuComponent.destroy()
        }

        function test_rotatedBoundingItem() {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var pieMenuComponent = Qt.createComponent("PieMenuRotatedBoundingItem.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            mouseClick(root, root.width / 2, root.height / 2, Qt.RightButton);
            tryCompare(pieMenu, "visible", true);

            mouseMove(root, 230, 145);
            // Not all styles have titles.
            if (pieMenu.__style.title)
                tryCompare(pieMenu.__panel.titleItem, "text", "Action 1");

            actionSignalSpy.signalName = "actionTriggered";
            actionSignalSpy.target = root;
            mouseClick(root, 230, 145);
            compare(actionSignalSpy.count, 1);
            compare(actionSignalSpy.signalArguments[0][0], 0);

            pieMenuComponent.destroy()
        }

        function test_boundingItem() {
            var oldContainerWidth = container.width;
            var oldContainerHeight = container.height;
            container.width = 560;
            container.height = 560;

            // Tests boundingItem when there are nested margins.
            var pieMenuComponent = Qt.createComponent("PieMenuBoundingItem.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var mouseArea = root.mouseArea;
            var pieMenu = root.pieMenu;

            var mouseAreaAbsolutePos = mouseArea.mapToItem(root, 0, 0);
            mouseClick(root, mouseAreaAbsolutePos.x + mouseArea.width / 2, mouseAreaAbsolutePos.y, Qt.RightButton);
            compare(pieMenu.visible, true);
            compare(pieMenu.y, -root.margins);

            mouseClick(root, 0, 0);
            compare(pieMenu.visible, false);

            var mouseAreaCenterAbsolutePos = mouseArea.mapToItem(root, mouseArea.width / 2, mouseArea.height / 2);
            mouseClick(root, mouseAreaCenterAbsolutePos.x, mouseAreaCenterAbsolutePos.y, Qt.RightButton);
            compare(pieMenu.visible, true);
            compare(pieMenu.mapToItem(root, 0, 0).x, mouseAreaCenterAbsolutePos.x - pieMenu.width / 2);
            compare(pieMenu.mapToItem(root, 0, 0).y, mouseAreaCenterAbsolutePos.y - pieMenu.height / 2);

            container.width = oldContainerWidth;
            container.height = oldContainerHeight;

            pieMenuComponent.destroy()
        }

        function test_longPressTriggerOnClick() {
            // Tests that a menu that is opened on press or long press does not
            // get closed by a release event when the triggerMode requires a
            // press before the release (TriggerOnClick).
            var pieMenuComponent = Qt.createComponent("PieMenu3ItemsLongPress.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var pieMenu = root.pieMenu;

            mousePress(root, 0, 0, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(pieMenu.__mouseThief.receivedPressEvent, false);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            mouseRelease(root, 0, 0, Qt.LeftButton);
            compare(pieMenu.visible, true);
            compare(pieMenu.__mouseThief.receivedPressEvent, false);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            mousePress(root, 0, 0, Qt.LeftButton);
            compare(pieMenu.visible, true);
            compare(pieMenu.__mouseThief.receivedPressEvent, true);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            mouseRelease(root, 0, 0, Qt.LeftButton);
            compare(pieMenu.visible, false);
            compare(pieMenu.__mouseThief.receivedPressEvent, false);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            pieMenuComponent.destroy()
        }

        function test_keepMenuOpenWhenTriggered() {
            // This functionality is used in the flat example.
            var pieMenuComponent = Qt.createComponent("PieMenu3ItemsKeepOpen.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            var pieMenu = root.pieMenu;
            actionSignalSpy.signalName = "actionTriggered";
            actionSignalSpy.target = root;

            mouseClick(root, 0, 0, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);

            mouseClick(root, pieMenu.x + pieMenu.width / 4, pieMenu.y + pieMenu.height / 4, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(actionSignalSpy.count, 1);
            compare(actionSignalSpy.signalArguments[0][0], 0);

            actionSignalSpy.clear();
            mouseClick(root, pieMenu.x + pieMenu.width / 2, pieMenu.y + pieMenu.height / 4, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(actionSignalSpy.count, 1);
            compare(actionSignalSpy.signalArguments[0][0], 1);

            actionSignalSpy.clear();
            mouseClick(root, pieMenu.x + pieMenu.width * 0.75, pieMenu.y + pieMenu.height / 4, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(actionSignalSpy.count, 1);
            compare(actionSignalSpy.signalArguments[0][0], 2);

            pieMenuComponent.destroy()
        }

        function test_pressedIndex() {
            var pieMenuComponent = Qt.createComponent("PieMenu3Items.qml");
            tryCompare(pieMenuComponent, "status", Component.Ready);
            root = pieMenuComponent.createObject(container);
            pieMenu = root.pieMenu;
            actionSignalSpy.signalName = "actionTriggered";
            actionSignalSpy.target = root;

            mouseClick(root, 0, 0, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            mousePress(root, pieMenu.x + pieMenu.width / 4, pieMenu.y + pieMenu.height / 4, Qt.LeftButton);
            tryCompare(pieMenu, "visible", true);
            compare(pieMenu.__protectedScope.pressedIndex, 0);

            mouseRelease(root, pieMenu.x + pieMenu.width / 4, pieMenu.y + pieMenu.height / 4, Qt.LeftButton);
            tryCompare(pieMenu, "visible", false);
            compare(actionSignalSpy.count, 1);
            compare(actionSignalSpy.signalArguments[0][0], 0);
            compare(pieMenu.__protectedScope.pressedIndex, -1);

            pieMenuComponent.destroy()
        }
    }
}
