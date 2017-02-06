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

import QtQuick 2.4
import QtTest 1.0
import QtQuick.Controls 1.4
import QtQuick.Controls.Private 1.0
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 400
    height: 400

    TestCase {
        id: testCase
        name: "Tests_TreeView"
        when:windowShown
        width:400
        height:400
        objectName: "testCase"

        SignalSpy {
            id: spy
        }

        Component {
            id: newColumn
            TableViewColumn {
                role: "name"
                title: "Name"
            }
        }

        property var instance_selectionModel: 'import QtQml.Models 2.2; ItemSelectionModel {}'
        property var semiIndent: 20/2 // PM_TreeViewIndentation 20 in commonStyle

        function cleanup()
        {
            // Make sure to delete all children even when the test has failed.
            for (var child in container.children) {
                if (container.children[child].objectName !== "testCase")
                    container.children[child].destroy()
            }
        }

        function test_basic_setup()
        {
            var test_instanceStr =
                    'import QtQuick 2.4;             \
                     import QtQuick.Controls 1.4;    \
                     import QtQuickControlsTests 1.0;\
                     TreeView {                      \
                         model: TreeModel {}         \
                         TableViewColumn {           \
                             role: "display";        \
                             title: "Default";       \
                         }                           \
                     }'

            var tree = Qt.createQmlObject(test_instanceStr, container, '')
            verify(!tree.currentIndex.valid)
            compare(tree.columnCount, 1)
            tree.addColumn(newColumn)
            compare(tree.columnCount, 2)
        }

        function test_clicked_signals()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            tree.forceActiveFocus()

            verify(!tree.currentIndex.valid)
            spy.clear()
            spy.target = tree
            spy.signalName = "clicked"
            compare(spy.count, 0)
            mouseClick(tree, semiIndent + 50, 120, Qt.LeftButton)
            compare(spy.count, 1)
            var clickedItem = spy.signalArguments[0][0]
            verify(clickedItem.valid)
            compare(clickedItem.row, 1)
            compare(tree.currentIndex.row, 1)
            compare(clickedItem.internalId, tree.currentIndex.internalId)

            spy.clear()
            spy.target = tree
            spy.signalName = "doubleClicked"
            compare(spy.count, 0)
            mouseDoubleClickSequence(tree, semiIndent + 50, 220, Qt.LeftButton)
            compare(spy.count, 1)
            clickedItem = spy.signalArguments[0][0]
            verify(clickedItem.valid)
            compare(clickedItem.row, 3)
            compare(clickedItem.internalId, tree.currentIndex.internalId)

            spy.clear()
            spy.target = tree
            spy.signalName = "activated"
            compare(spy.count, 0)
            if (!tree.__activateItemOnSingleClick)
                mouseDoubleClickSequence(tree, semiIndent + 50 , 120, Qt.LeftButton)
            else
                mouseClick(tree, semiIndent + 50, 120, Qt.LeftButton)
            compare(spy.count, 1)
            clickedItem = spy.signalArguments[0][0]
            verify(clickedItem.valid)
            compare(clickedItem.row, 1)
            compare(clickedItem.internalId, tree.currentIndex.internalId)
        }

        function test_headerHidden()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            tree.headerVisible = false
            tree.forceActiveFocus()

            verify(!tree.currentIndex.valid)
            spy.clear()
            spy.target = tree
            spy.signalName = "clicked"
            compare(spy.count, 0)
            mouseClick(tree, semiIndent + 50, 20, Qt.LeftButton)
            compare(spy.count, 1)
            verify(spy.signalArguments[0][0].valid)
            compare(spy.signalArguments[0][0].row, 0)
            compare(tree.currentIndex.row, 0)
        }

        function test_expand_collapse()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)
            tree.forceActiveFocus()

            spy.clear()
            spy.target = tree
            spy.signalName = "expanded"

            // expanded on click
            compare(spy.count, 0)
            mouseClick(tree, semiIndent, 70, Qt.LeftButton)
            compare(spy.count, 1)
            var expandedIndex = spy.signalArguments[0][0]
            verify(expandedIndex.valid)
            compare(expandedIndex.row, 0)
            compare(tree.isExpanded(expandedIndex), true)

            // expand first child on click
            mouseClick(tree, semiIndent * 3, 120, Qt.LeftButton)
            compare(spy.count, 2)
            var childIndex = spy.signalArguments[1][0]
            verify(childIndex.valid)
            compare(childIndex.row, 0)
            compare(tree.isExpanded(childIndex), true)
            compare(childIndex.parent.internalId, expandedIndex.internalId)

            spy.clear()
            spy.signalName = "collapsed"

            // collapsed on click top item
            compare(spy.count, 0)
            mouseClick(tree, semiIndent, 70, Qt.LeftButton)
            compare(spy.count, 1)
            var collapsedIndex = spy.signalArguments[0][0]
            verify(collapsedIndex.valid)
            compare(collapsedIndex.row, 0)
            compare(tree.isExpanded(collapsedIndex), false)
            compare(expandedIndex.internalId, collapsedIndex.internalId)

            // check hidden child is still expanded
            compare(tree.isExpanded(childIndex), true)

            // collapse child with function
            tree.collapse(childIndex)
            compare(tree.isExpanded(childIndex), false)
            compare(spy.count, 2)
            compare(spy.signalArguments[1][0].row, 0)

            spy.clear()
            spy.signalName = "expanded"
            compare(spy.count, 0)

            // expand child with function
            tree.expand(expandedIndex)
            compare(tree.isExpanded(expandedIndex), true)
            compare(spy.count, 1)
            compare(spy.signalArguments[0][0].row, 0)
        }

        function test_pressAndHold()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            tree.forceActiveFocus()

            var styleIndent = !!tree.style.indentation ? tree.style.indentation/2 : 6
            verify(!tree.currentIndex.valid)
            spy.clear()
            spy.target = tree
            spy.signalName = "pressAndHold"
            compare(spy.count, 0)
            mousePress(tree, styleIndent + 50, 70, Qt.LeftButton)
            mouseRelease(tree, styleIndent + 50, 70, Qt.LeftButton, Qt.NoModifier, 1000)
            compare(spy.count, 1)
            verify(spy.signalArguments[0][0].valid)
            compare(spy.signalArguments[0][0].row, 0)
            compare(tree.currentIndex.row, 0)
        }

        function test_keys_navigation()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_2.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            tree.forceActiveFocus()

            // select second item with no children
            verify(!tree.currentIndex.valid)
            mouseClick(tree, semiIndent + 50, 120, Qt.LeftButton)
            var secondTopItem = tree.currentIndex
            verify(secondTopItem.valid)
            verify(!secondTopItem.parent.valid)
            compare(secondTopItem.row, 1)

            // Press right (selected item is non expandable)
            compare(tree.collapsedCount, 0)
            compare(tree.expandedCount, 0)
            keyClick(Qt.Key_Right)
            compare(tree.collapsedCount, 0)
            compare(tree.expandedCount, 0)
            compare(tree.currentIndex, secondTopItem)

            // Going down
            keyClick(Qt.Key_Down)
            var thirdTopItem = tree.currentIndex
            compare(thirdTopItem.row, 2)
            verify(!thirdTopItem.parent.valid)

            // Press right - expand - go down - go up - collapse
            keyClick(Qt.Key_Right)
            compare(tree.collapsedCount, 0)
            compare(tree.expandedCount, 1)
            compare(tree.isExpanded(thirdTopItem), true)
            keyClick(Qt.Key_Down)
            var firstChild_thirdTopItem = tree.currentIndex
            compare(firstChild_thirdTopItem.row, 0)
            verify(firstChild_thirdTopItem.parent.valid)
            compare(firstChild_thirdTopItem.parent.row, 2)
            compare(firstChild_thirdTopItem.parent.internalId, thirdTopItem.internalId)
            keyClick(Qt.Key_Up)
            verify(!tree.currentIndex.parent.valid)
            compare(tree.currentIndex.internalId, thirdTopItem.internalId)
            compare(tree.currentIndex.row, 2)
            compare(tree.isExpanded(tree.currentIndex), true)
            keyClick(Qt.Key_Left)
            compare(tree.isExpanded(tree.currentIndex), false)
            compare(tree.collapsedCount, 1)
            compare(tree.expandedCount, 1)
        }

        function test_selection_singleSelection()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model

            // Collect some model index
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            var firstItem = tree.currentIndex
            verify(firstItem.valid)
            compare(firstItem.row, 0)
            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            var secondItem = tree.currentIndex
            verify(secondItem.valid)
            compare(secondItem.row, 1)
            mouseClick(tree, semiIndent + 50, 20 + 3*50, Qt.LeftButton)
            var thirdItem = tree.currentIndex
            verify(thirdItem.valid)
            compare(thirdItem.row, 2)
            mouseClick(tree, semiIndent + 50, 20 + 4*50, Qt.LeftButton)
            var fourthItem = tree.currentIndex
            verify(fourthItem.valid)
            compare(fourthItem.row, 3)
            mouseClick(tree, semiIndent + 50, 20 + 5*50, Qt.LeftButton)
            var fifthItem = tree.currentIndex
            verify(fifthItem.valid)
            compare(fifthItem.row, 4)
            mouseClick(tree, semiIndent + 50, 20 + 6*50, Qt.LeftButton)
            var sixthItem = tree.currentIndex
            verify(sixthItem.valid)
            compare(sixthItem.row, 5)

            compare(tree.selection, null)
            tree.selection = selectionModel
            compare(tree.selection, selectionModel)
            tree.selection.clear()
            compare(tree.selection.hasSelection, false)

            //// Single selectionModel
            compare(tree.selectionMode, SelectionMode.SingleSelection)
            verify(!tree.selection.currentIndex.valid)

            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            verify(tree.selection.currentIndex.valid)

            compare(secondItem.internalId, tree.currentIndex.internalId)
            compare(secondItem.internalId, tree.selection.currentIndex.internalId)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.hasSelection, true)
            var list = tree.selection.selectedIndexes
            compare(list.length, 1)
            if (list.length === 1) {
                compare(list[0].internalId, secondItem.internalId)
                compare(tree.selection.isSelected(secondItem), true)
            }

            keyClick(Qt.Key_Down, Qt.ShiftModifier)
            compare(thirdItem.internalId, tree.currentIndex.internalId)
            compare(thirdItem.internalId, tree.selection.currentIndex.internalId)

            keyClick(Qt.Key_Down, Qt.ControlModifier)
            compare(fourthItem.internalId, tree.currentIndex.internalId)
            expectFailContinue('', 'BUG selected state not updated with Command/Control when SingleSelection')
            compare(fourthItem.internalId, tree.selection.currentIndex.internalId)
            expectFailContinue('', 'BUG selected state not updated with Command/Control when SingleSelection')
            compare(tree.selection.isSelected(fourthItem), true)
        }

        function test_selection_noSelection()
        {
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model

            // Collect some model index
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            var firstItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            var secondItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 3*50, Qt.LeftButton)
            var thirdItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 4*50, Qt.LeftButton)
            var fourthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 5*50, Qt.LeftButton)
            var fifthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 6*50, Qt.LeftButton)
            var sixthItem = tree.currentIndex

            compare(tree.selection, null)
            tree.selection = selectionModel
            compare(tree.selection, selectionModel)
            tree.selection.clear()
            compare(tree.selection.hasSelection, false)

            //// No selection
            tree.selectionMode = SelectionMode.NoSelection
            compare(tree.selectionMode, SelectionMode.NoSelection)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)

            compare(secondItem.internalId, tree.currentIndex.internalId)
            verify(!tree.selection.currentIndex.valid)
            compare(tree.selection.hasSelection, false)
            compare(tree.selection.isSelected(secondItem), false)

            keyClick(Qt.Key_Down, Qt.ShiftModifier)
            verify(!tree.selection.currentIndex.valid)
            compare(tree.selection.hasSelection, false)
            compare(tree.selection.isSelected(thirdItem), false)

            keyClick(Qt.Key_Down, Qt.ControlModifier)
            verify(!tree.selection.currentIndex.valid)
            compare(tree.selection.hasSelection, false)
        }

        function test_selection_multiSelection()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model

            // Collect some model index
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            var firstItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            var secondItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 3*50, Qt.LeftButton)
            var thirdItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 4*50, Qt.LeftButton)
            var fourthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 5*50, Qt.LeftButton)
            var fifthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 6*50, Qt.LeftButton)
            var sixthItem = tree.currentIndex

            compare(tree.selection, null)
            tree.selection = selectionModel
            compare(tree.selection, selectionModel)
            tree.selection.clear()
            compare(tree.selection.hasSelection, false)

            ////// Multi selection
            tree.selectionMode = SelectionMode.MultiSelection
            compare(tree.selectionMode, SelectionMode.MultiSelection)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)

            compare(secondItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            var listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 1)
            compare(listIndexes[0].internalId, secondItem.internalId)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, secondItem.internalId)

            mouseClick(tree, semiIndent + 50, 70+150, Qt.LeftButton)
            compare(fourthItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 2)
            compare(listIndexes[0].internalId, secondItem.internalId)
            compare(listIndexes[1].internalId, fourthItem.internalId)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, fourthItem.internalId)

            keyPress(Qt.Key_Shift)
            mouseClick(tree, semiIndent + 50, 70+250, Qt.LeftButton)
            keyRelease(Qt.Key_Shift)
            compare(sixthItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(fifthItem), false)
            compare(tree.selection.isSelected(sixthItem), true)

            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 3)
            compare(listIndexes[0].internalId, secondItem.internalId)
            compare(listIndexes[1].internalId, fourthItem.internalId)
            compare(listIndexes[2].internalId, sixthItem.internalId)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, sixthItem.internalId)


            mouseClick(tree, semiIndent + 50, 70+150, Qt.LeftButton)
            compare(fourthItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(fourthItem), false)
            compare(tree.selection.isSelected(sixthItem), true)

            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 2)
            compare(listIndexes[0].internalId, secondItem.internalId)
            compare(listIndexes[1].internalId, sixthItem.internalId)
            verify(tree.selection.currentIndex.valid)

            mouseClick(tree, semiIndent + 50, 70+150, Qt.LeftButton)
            compare(fourthItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(sixthItem), true)

            keyPress(Qt.Key_Shift)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Down)
            keyRelease(Qt.Key_Shift)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(fifthItem), true)
            compare(tree.selection.isSelected(sixthItem), false)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 4)
        }

        function test_selection_extendedSelection()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model

            // Collect some model index
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            var firstItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            var secondItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 3*50, Qt.LeftButton)
            var thirdItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 4*50, Qt.LeftButton)
            var fourthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 5*50, Qt.LeftButton)
            var fifthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 6*50, Qt.LeftButton)
            var sixthItem = tree.currentIndex

            compare(tree.selection, null)
            tree.selection = selectionModel
            compare(tree.selection, selectionModel)
            tree.selection.clear()
            compare(tree.selection.hasSelection, false)

            ////// Extended selection
            tree.selectionMode = SelectionMode.ExtendedSelection
            compare(tree.selectionMode, SelectionMode.ExtendedSelection)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)

            compare(secondItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            var listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 1)
            compare(listIndexes[0].internalId, secondItem.internalId)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, secondItem.internalId)

            // Re-click does not deselect
            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            // Ctrl/Cmd click deselect
            mouseClick(tree, semiIndent + 50, 70+52, Qt.LeftButton, Qt.ControlModifier)
            compare(tree.selection.hasSelection, false)
            compare(tree.selection.isSelected(secondItem), false)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 0)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)
            keyPress(Qt.Key_Down, Qt.ShiftModifier)
            keyPress(Qt.Key_Down, Qt.ShiftModifier)
            keyClick(Qt.Key_Down, Qt.ShiftModifier)

            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 4)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(thirdItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(fifthItem), true)

            mouseClick(tree, semiIndent + 50, 70+300, Qt.LeftButton, Qt.ShiftModifier)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 6)

            mouseClick(tree, semiIndent + 50, 70+150, Qt.LeftButton, Qt.ControlModifier)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 5)
            compare(tree.selection.isSelected(fourthItem), false)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(thirdItem), true)
            compare(tree.selection.isSelected(sixthItem), true)
            compare(tree.selection.isSelected(fifthItem), true)
        }

        function test_selection_contiguousSelection()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model

            // Collect some model index
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            var firstItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 2*50, Qt.LeftButton)
            var secondItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 3*50, Qt.LeftButton)
            var thirdItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 4*50, Qt.LeftButton)
            var fourthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 5*50, Qt.LeftButton)
            var fifthItem = tree.currentIndex
            mouseClick(tree, semiIndent + 50, 20 + 6*50, Qt.LeftButton)
            var sixthItem = tree.currentIndex

            compare(tree.selection, null)
            tree.selection = selectionModel
            compare(tree.selection, selectionModel)
            tree.selection.clear()
            compare(tree.selection.hasSelection, false)

            ////// Contiguous selection
            tree.selectionMode = SelectionMode.ContiguousSelection
            compare(tree.selectionMode, SelectionMode.ContiguousSelection)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)

            compare(secondItem.internalId, tree.currentIndex.internalId)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            var listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 1)
            compare(listIndexes[0].internalId, secondItem.internalId)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, secondItem.internalId)

            // Re-click does not deselect
            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            // Ctrl/Cmd click deselect all, selects current
            mouseClick(tree, semiIndent + 50, 70+52, Qt.LeftButton, Qt.ControlModifier)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 1)

            mouseClick(tree, semiIndent + 50, 70+50, Qt.LeftButton)
            keyPress(Qt.Key_Down, Qt.ShiftModifier)
            keyPress(Qt.Key_Down, Qt.ShiftModifier)
            keyClick(Qt.Key_Down, Qt.ShiftModifier)

            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 4)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(thirdItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(fifthItem), true)
            verify(tree.selection.currentIndex.valid)
            if (tree.selection.currentIndex.valid)
                compare(tree.selection.currentIndex.internalId, fifthItem.internalId)

            mouseClick(tree, semiIndent + 50, 70+300, Qt.LeftButton, Qt.ShiftModifier)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 6)

            mouseClick(tree, semiIndent + 50, 70+150, Qt.LeftButton, Qt.ShiftModifier)

            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 3)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(secondItem), true)
            compare(tree.selection.isSelected(thirdItem), true)
            compare(tree.selection.isSelected(fourthItem), true)
            compare(tree.selection.isSelected(fifthItem), false)
            compare(tree.selection.isSelected(sixthItem), false)

            mouseClick(tree, semiIndent + 50, 70+100, Qt.LeftButton)
            listIndexes = tree.selection.selectedIndexes
            compare(listIndexes.length, 1)
            compare(tree.selection.hasSelection, true)
            compare(tree.selection.isSelected(thirdItem), true)
        }

        function test_indexAt() {
            skip("Fails because of bug QTBUG-47523")
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            verify(waitForRendering(tree), "TreeView did not render on time")
            var model = tree.model

            // Sample each row and test
            for (var row = 0; row < tree.__listView.count; row++) {
                for (var x = 1; x < tree.getColumn(0).width; x += 10) {
                    var treeIndex = tree.indexAt(x, 50 * (row + 1) + 1) // offset by header height
                    var modelIndex = model.index(row, 0)
                    if (treeIndex.row !== modelIndex.row
                        || treeIndex.column !== modelIndex.column
                        || treeIndex.internalId !== modelIndex.internalId) {
                        console.log("Test about to fail: row = " + row + ", __listView.count =" + tree.__listView.count)
                        console.log(" . . . . . . . . .  x =" + x + ", getColumn(0).width =" + tree.getColumn(0).width)
                    }
                    compare(treeIndex.row, modelIndex.row)
                    compare(treeIndex.column, modelIndex.column)
                    compare(treeIndex.internalId, modelIndex.internalId)
                }
            }

            // Hide header, test again
            tree.headerVisible = false
            for (row = 0; row < tree.__listView.count; row++) {
                for (x = 1; x < tree.getColumn(0).width; x += 10) {
                    treeIndex = tree.indexAt(x, 50 * row + 1)
                    modelIndex = model.index(row, 0)
                    if (treeIndex.row !== modelIndex.row
                        || treeIndex.column !== modelIndex.column
                        || treeIndex.internalId !== modelIndex.internalId) {
                        console.log("Test about to fail: row = " + row + ", __listView.count =" + __listView.count)
                        console.log(" . . . . . . . . .  x =" + x + ", getColumn(0).width =" + getColumn(0).width)
                    }
                    compare(treeIndex.row, modelIndex.row)
                    compare(treeIndex.column, modelIndex.column)
                    compare(treeIndex.internalId, modelIndex.internalId)
                }
            }

            // Test outside the view content area
            modelIndex = model.index(-1,-1)

            treeIndex = tree.indexAt(-10, 55)
            compare(treeIndex.row, modelIndex.row)
            compare(treeIndex.column, modelIndex.column)
            compare(treeIndex.internalId, modelIndex.internalId)

            treeIndex = tree.indexAt(-10, tree.getColumn(0).width + 10)
            compare(treeIndex.row, modelIndex.row)
            compare(treeIndex.column, modelIndex.column)
            compare(treeIndex.internalId, modelIndex.internalId)

            treeIndex = tree.indexAt(10, -10)
            compare(treeIndex.row, modelIndex.row)
            compare(treeIndex.column, modelIndex.column)
            compare(treeIndex.internalId, modelIndex.internalId)

            treeIndex = tree.indexAt(10, tree.__listView.contentHeight + 10)
            compare(treeIndex.row, modelIndex.row)
            compare(treeIndex.column, modelIndex.column)
            compare(treeIndex.internalId, modelIndex.internalId)
        }

        function test_QTBUG_46891_selection_collapse_parent()
        {
            if (Settings.hasTouchScreen)
                skip("Fails with touch screens");
            var component = Qt.createComponent("treeview/treeview_1.qml")
            compare(component.status, Component.Ready)
            var tree = component.createObject(container);
            verify(tree !== null, "tree created is null")
            var model = tree.model
            model.removeRows(1, 9)
            model.removeRows(1, 9, model.index(0, 0))
            waitForRendering(tree)

            var selectionModel = Qt.createQmlObject(testCase.instance_selectionModel, container, '')
            selectionModel.model = tree.model
            tree.selection = selectionModel
            tree.selectionMode = SelectionMode.ExtendedSelection

            var parentItem = tree.model.index(0, 0)
            tree.expand(parentItem)
            verify(tree.isExpanded(parentItem))

            wait(100)
            mouseClick(tree, semiIndent + 50, 20 + 100, Qt.LeftButton)
            verify(selectionModel.isSelected(tree.currentIndex))

            tree.collapse(parentItem)
            mouseClick(tree, semiIndent + 50, 20 + 50, Qt.LeftButton)
            verify(selectionModel.isSelected(parentItem))
        }
    }
}
