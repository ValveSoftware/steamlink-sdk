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
import QtQuick.Window 2.2
import QtTest 1.0
import QtQuick.Controls 2.1

TestCase {
    id: testCase
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "ComboBox"

    Component {
        id: signalSpy
        SignalSpy { }
    }

    Component {
        id: comboBox
        ComboBox { }
    }

    Component {
        id: emptyBox
        ComboBox {
            delegate: ItemDelegate {
                width: popup.width
            }
        }
    }

    function test_defaults() {
        var control = comboBox.createObject(testCase)
        verify(control)

        compare(control.count, 0)
        compare(control.model, undefined)
        compare(control.flat, false)
        compare(control.pressed, false)
        compare(control.currentIndex, -1)
        compare(control.highlightedIndex, -1)
        compare(control.currentText, "")
        verify(control.delegate)
        verify(control.indicator)
        verify(control.popup)

        control.destroy()
    }

    function test_array() {
        var control = comboBox.createObject(testCase)
        verify(control)

        var items = [ "Banana", "Apple", "Coconut" ]

        control.model = items
        compare(control.model, items)

        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentText, "Banana")

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentText, "Coconut")

        control.model = null
        compare(control.model, null)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentText, "")

        control.destroy()
    }

    function test_objects() {
        var control = emptyBox.createObject(testCase)
        verify(control)

        var items = [
            { text: "Apple" },
            { text: "Orange" },
            { text: "Banana" }
        ]

        control.model = items
        compare(control.model, items)

        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentText, "Apple")

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentText, "Banana")

        control.model = null
        compare(control.model, null)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentText, "")

        control.destroy()
    }

    function test_number() {
        var control = comboBox.createObject(testCase)
        verify(control)

        control.model = 10
        compare(control.model, 10)

        compare(control.count, 10)
        compare(control.currentIndex, 0)
        compare(control.currentText, "0")

        control.currentIndex = 9
        compare(control.currentIndex, 9)
        compare(control.currentText, "9")

        control.model = 0
        compare(control.model, 0)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentText, "")

        control.destroy()
    }

    ListModel {
        id: listmodel
        ListElement { text: "First" }
        ListElement { text: "Second" }
        ListElement { text: "Third" }
        ListElement { text: "Fourth" }
        ListElement { text: "Fifth" }
    }

    function test_listModel() {
        var control = comboBox.createObject(testCase)
        verify(control)

        control.model = listmodel
        compare(control.model, listmodel)

        compare(control.count, 5)
        compare(control.currentIndex, 0)
        compare(control.currentText, "First")

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentText, "Third")

        control.model = undefined
        compare(control.model, undefined)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentText, "")

        control.destroy()
    }

    ListModel {
        id: fruitmodel
        ListElement { name: "Apple"; color: "red" }
        ListElement { name: "Orange"; color: "orange" }
        ListElement { name: "Banana"; color: "yellow" }
    }

    property var fruitarray: [
        { name: "Apple", color: "red" },
        { name: "Orange", color: "orange" },
        { name: "Banana", color: "yellow" }
    ]

    function test_textRole_data() {
        return [
            { tag: "ListModel", model: fruitmodel },
            { tag: "ObjectArray", model: fruitarray }
        ]
    }

    function test_textRole(data) {
        var control = emptyBox.createObject(testCase)
        verify(control)

        control.model = data.model
        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentText, "")

        control.textRole = "name"
        compare(control.currentText, "Apple")

        control.textRole = "color"
        compare(control.currentText, "red")

        control.currentIndex = 1
        compare(control.currentIndex, 1)
        compare(control.currentText, "orange")

        control.textRole = "name"
        compare(control.currentText, "Orange")

        control.textRole = ""
        compare(control.currentText, "")

        control.destroy()
    }

    function test_textAt() {
        var control = comboBox.createObject(testCase)
        verify(control)

        control.model = ["Apple", "Orange", "Banana"]
        compare(control.textAt(0), "Apple")
        compare(control.textAt(1), "Orange")
        compare(control.textAt(2), "Banana")
        compare(control.textAt(-1), "") // TODO: null?
        compare(control.textAt(5), "") // TODO: null?

        control.destroy()
    }

    function test_find_data() {
        return [
            { tag: "Banana (MatchExactly)", term: "Banana", flags: Qt.MatchExactly, index: 0 },
            { tag: "banana (MatchExactly)", term: "banana", flags: Qt.MatchExactly, index: 1 },
            { tag: "bananas (MatchExactly)", term: "bananas", flags: Qt.MatchExactly, index: -1 },
            { tag: "Cocomuffin (MatchExactly)", term: "Cocomuffin", flags: Qt.MatchExactly, index: 4 },

            { tag: "b(an)+a (MatchRegExp)", term: "B(an)+a", flags: Qt.MatchRegExp, index: 0 },
            { tag: "b(an)+a (MatchRegExp|MatchCaseSensitive)", term: "b(an)+a", flags: Qt.MatchRegExp | Qt.MatchCaseSensitive, index: 1 },
            { tag: "[coc]+\\w+ (MatchRegExp)", term: "[coc]+\\w+", flags: Qt.MatchRegExp, index: 2 },

            { tag: "?pp* (MatchWildcard)", term: "?pp*", flags: Qt.MatchWildcard, index: 3 },
            { tag: "app* (MatchWildcard|MatchCaseSensitive)", term: "app*", flags: Qt.MatchWildcard | Qt.MatchCaseSensitive, index: -1 },

            { tag: "Banana (MatchFixedString)", term: "Banana", flags: Qt.MatchFixedString, index: 0 },
            { tag: "banana (MatchFixedString|MatchCaseSensitive)", term: "banana", flags: Qt.MatchFixedString | Qt.MatchCaseSensitive, index: 1 },

            { tag: "coco (MatchStartsWith)", term: "coco", flags: Qt.MatchStartsWith, index: 2 },
            { tag: "coco (MatchStartsWith|MatchCaseSensitive)", term: "coco", flags: Qt.StartsWith | Qt.MatchCaseSensitive, index: -1 },

            { tag: "MUFFIN (MatchEndsWith)", term: "MUFFIN", flags: Qt.MatchEndsWith, index: 4 },
            { tag: "MUFFIN (MatchEndsWith|MatchCaseSensitive)", term: "MUFFIN", flags: Qt.MatchEndsWith | Qt.MatchCaseSensitive, index: -1 },

            { tag: "Con (MatchContains)", term: "Con", flags: Qt.MatchContains, index: 2 },
            { tag: "Con (MatchContains|MatchCaseSensitive)", term: "Con", flags: Qt.MatchContains | Qt.MatchCaseSensitive, index: -1 },
        ]
    }

    function test_find(data) {
        var control = comboBox.createObject(testCase)
        verify(control)

        control.model = ["Banana", "banana", "Coconut", "Apple", "Cocomuffin"]

        compare(control.find(data.term, data.flags), data.index)

        control.destroy()
    }


    function test_arrowKeys() {
        var control = comboBox.createObject(testCase, {model: 3})
        verify(control)

        var activatedSpy = signalSpy.createObject(control, {target: control, signalName: "activated"})
        verify(activatedSpy.valid)

        var highlightedSpy = signalSpy.createObject(control, {target: control, signalName: "highlighted"})
        verify(highlightedSpy.valid)

        waitForRendering(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, -1)

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 1)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 1)
        activatedSpy.clear()

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 2)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 2)
        activatedSpy.clear()

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 2)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 0)

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 1)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 1)
        activatedSpy.clear()

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 1)
        compare(activatedSpy.signalArguments[0][0], 0)
        activatedSpy.clear()

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, -1)
        compare(highlightedSpy.count, 0)
        compare(activatedSpy.count, 0)

        // show popup
        keyClick(Qt.Key_Space)

        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 0)

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 1)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 1)
        compare(highlightedSpy.signalArguments[0][0], 1)
        highlightedSpy.clear()

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 2)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 1)
        compare(highlightedSpy.signalArguments[0][0], 2)
        highlightedSpy.clear()

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 2)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 1)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 1)
        compare(highlightedSpy.signalArguments[0][0], 1)
        highlightedSpy.clear()

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 0)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 1)
        compare(highlightedSpy.signalArguments[0][0], 0)
        highlightedSpy.clear()

        keyClick(Qt.Key_Up)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 0)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)

        keyClick(Qt.Key_Down)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, 1)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 1)
        compare(highlightedSpy.signalArguments[0][0], 1)
        highlightedSpy.clear()

        // hide popup
        keyClick(Qt.Key_Space)

        compare(control.currentIndex, 1)
        tryCompare(control, "highlightedIndex", -1)

        control.destroy()
    }

    function test_keys_space_enter_escape_data() {
        return [
            { tag: "space-space", key1: Qt.Key_Space, key2: Qt.Key_Space, showPopup: true, showPress: true, hidePopup: true, hidePress: true },
            { tag: "space-enter", key1: Qt.Key_Space, key2: Qt.Key_Enter, showPopup: true, showPress: true, hidePopup: true, hidePress: true },
            { tag: "space-return", key1: Qt.Key_Space, key2: Qt.Key_Return, showPopup: true, showPress: true, hidePopup: true, hidePress: true },
            { tag: "space-escape", key1: Qt.Key_Space, key2: Qt.Key_Escape, showPopup: true, showPress: true, hidePopup: true, hidePress: false },
            { tag: "space-0", key1: Qt.Key_Space, key2: Qt.Key_0, showPopup: true, showPress: true, hidePopup: false, hidePress: false },
            { tag: "enter-enter", key1: Qt.Key_Enter, key2: Qt.Key_Enter, showPopup: false, showPress: false, hidePopup: true, hidePress: false },
            { tag: "return-return", key1: Qt.Key_Return, key2: Qt.Key_Return, showPopup: false, showPress: false, hidePopup: true, hidePress: false },
            { tag: "escape-escape", key1: Qt.Key_Escape, key2: Qt.Key_Escape, showPopup: false, showPress: false, hidePopup: true, hidePress: false }
        ]
    }

    function test_keys_space_enter_escape(data) {
        var control = comboBox.createObject(testCase, {model: 3})
        verify(control)

        waitForRendering(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        compare(control.pressed, false)
        compare(control.popup.visible, false)

        // show popup
        keyPress(data.key1)
        compare(control.pressed, data.showPress)
        compare(control.popup.visible, false)
        keyRelease(data.key1)
        compare(control.pressed, false)
        compare(control.popup.visible, data.showPopup)

        // hide popup
        keyPress(data.key2)
        compare(control.pressed, data.hidePress)
        keyRelease(data.key2)
        compare(control.pressed, false)
        tryCompare(control.popup, "visible", !data.hidePopup)

        control.destroy()
    }

    function test_keys_home_end() {
        var control = comboBox.createObject(testCase, {model: 5})
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)
        compare(control.currentIndex, 0)
        compare(control.highlightedIndex, -1)

        var activatedCount = 0
        var activatedSpy = signalSpy.createObject(control, {target: control, signalName: "activated"})
        verify(activatedSpy.valid)

        var highlightedCount = 0
        var highlightedSpy = signalSpy.createObject(control, {target: control, signalName: "highlighted"})
        verify(highlightedSpy.valid)

        var currentIndexCount = 0
        var currentIndexSpy = signalSpy.createObject(control, {target: control, signalName: "currentIndexChanged"})
        verify(currentIndexSpy.valid)

        var highlightedIndexCount = 0
        var highlightedIndexSpy = signalSpy.createObject(control, {target: control, signalName: "highlightedIndexChanged"})
        verify(highlightedIndexSpy.valid)

        // end (popup closed)
        keyClick(Qt.Key_End)
        compare(control.currentIndex, 4)
        compare(currentIndexSpy.count, ++currentIndexCount)

        compare(control.highlightedIndex, -1)
        compare(highlightedIndexSpy.count, highlightedIndexCount)

        compare(activatedSpy.count, ++activatedCount)
        compare(activatedSpy.signalArguments[activatedCount-1][0], 4)

        compare(highlightedSpy.count, highlightedCount)

        // repeat (no changes/signals)
        keyClick(Qt.Key_End)
        compare(currentIndexSpy.count, currentIndexCount)
        compare(highlightedIndexSpy.count, highlightedIndexCount)
        compare(activatedSpy.count, activatedCount)
        compare(highlightedSpy.count, highlightedCount)

        // home (popup closed)
        keyClick(Qt.Key_Home)
        compare(control.currentIndex, 0)
        compare(currentIndexSpy.count, ++currentIndexCount)

        compare(control.highlightedIndex, -1)
        compare(highlightedIndexSpy.count, highlightedIndexCount)

        compare(activatedSpy.count, ++activatedCount)
        compare(activatedSpy.signalArguments[activatedCount-1][0], 0)

        compare(highlightedSpy.count, highlightedCount)

        // repeat (no changes/signals)
        keyClick(Qt.Key_Home)
        compare(currentIndexSpy.count, currentIndexCount)
        compare(highlightedIndexSpy.count, highlightedIndexCount)
        compare(activatedSpy.count, activatedCount)
        compare(highlightedSpy.count, highlightedCount)

        control.popup.open()
        compare(control.highlightedIndex, 0)
        compare(highlightedIndexSpy.count, ++highlightedIndexCount)
        compare(highlightedSpy.count, highlightedCount)

        // end (popup open)
        keyClick(Qt.Key_End)
        compare(control.currentIndex, 0)
        compare(currentIndexSpy.count, currentIndexCount)

        compare(control.highlightedIndex, 4)
        compare(highlightedIndexSpy.count, ++highlightedIndexCount)

        compare(activatedSpy.count, activatedCount)

        compare(highlightedSpy.count, ++highlightedCount)
        compare(highlightedSpy.signalArguments[highlightedCount-1][0], 4)

        // repeat (no changes/signals)
        keyClick(Qt.Key_End)
        compare(currentIndexSpy.count, currentIndexCount)
        compare(highlightedIndexSpy.count, highlightedIndexCount)
        compare(activatedSpy.count, activatedCount)
        compare(highlightedSpy.count, highlightedCount)

        // home (popup open)
        keyClick(Qt.Key_Home)
        compare(control.currentIndex, 0)
        compare(currentIndexSpy.count, currentIndexCount)

        compare(control.highlightedIndex, 0)
        compare(highlightedIndexSpy.count, ++highlightedIndexCount)

        compare(activatedSpy.count, activatedCount)

        compare(highlightedSpy.count, ++highlightedCount)
        compare(highlightedSpy.signalArguments[highlightedCount-1][0], 0)

        // repeat (no changes/signals)
        keyClick(Qt.Key_Home)
        compare(currentIndexSpy.count, currentIndexCount)
        compare(highlightedIndexSpy.count, highlightedIndexCount)
        compare(activatedSpy.count, activatedCount)
        compare(highlightedSpy.count, highlightedCount)

        control.destroy()
    }

    function test_keySearch() {
        var control = comboBox.createObject(testCase, {model: ["Banana", "Coco", "Coconut", "Apple", "Cocomuffin"]})
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        compare(control.currentIndex, 0)
        compare(control.currentText, "Banana")

        keyPress(Qt.Key_C)
        compare(control.currentIndex, 1)
        compare(control.currentText, "Coco")

        // no match
        keyPress(Qt.Key_N)
        compare(control.currentIndex, 1)
        compare(control.currentText, "Coco")

        keyPress(Qt.Key_C)
        compare(control.currentIndex, 2)
        compare(control.currentText, "Coconut")

        keyPress(Qt.Key_C)
        compare(control.currentIndex, 4)
        compare(control.currentText, "Cocomuffin")

        // wrap
        keyPress(Qt.Key_C)
        compare(control.currentIndex, 1)
        compare(control.currentText, "Coco")

        keyPress(Qt.Key_A)
        compare(control.currentIndex, 3)
        compare(control.currentText, "Apple")

        keyPress(Qt.Key_B)
        compare(control.currentIndex, 0)
        compare(control.currentText, "Banana")

        control.destroy()
    }

    function test_popup() {
        var control = comboBox.createObject(testCase, {model: 3})
        verify(control)

        // show below
        mousePress(control)
        compare(control.pressed, true)
        compare(control.popup.visible, false)
        mouseRelease(control)
        compare(control.pressed, false)
        compare(control.popup.visible, true)
        verify(control.popup.contentItem.y >= control.y)

        // hide
        mouseClick(control)
        compare(control.pressed, false)
        tryCompare(control.popup, "visible", false)

        // show above
        control.y = control.Window.height - control.height
        mousePress(control)
        compare(control.pressed, true)
        compare(control.popup.visible, false)
        mouseRelease(control)
        compare(control.pressed, false)
        compare(control.popup.visible, true)
        verify(control.popup.contentItem.y < control.y)

        // follow the control outside the horizontal window bounds
        control.x = -control.width / 2
        compare(control.x, -control.width / 2)
        compare(control.popup.contentItem.parent.x, -control.width / 2)
        control.x = testCase.width - control.width / 2
        compare(control.x, testCase.width - control.width / 2)
        compare(control.popup.contentItem.parent.x, testCase.width - control.width / 2)

        control.destroy()
    }

    function test_mouse() {
        var control = comboBox.createObject(testCase, {model: 3})
        verify(control)

        var activatedSpy = signalSpy.createObject(control, {target: control, signalName: "activated"})
        verify(activatedSpy.valid)

        var highlightedSpy = signalSpy.createObject(control, {target: control, signalName: "highlighted"})
        verify(highlightedSpy.valid)

        mouseClick(control)
        compare(control.popup.visible, true)

        var content = control.popup.contentItem
        waitForRendering(content)

        // press - move - release outside - not activated - not closed
        mousePress(content)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        mouseMove(content, content.width * 2)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        mouseRelease(content, content.width * 2)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        compare(control.popup.visible, true)

        // press - move - release inside - activated - closed
        mousePress(content)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        mouseMove(content, content.width / 2 + 1, content.height / 2 + 1)
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        mouseRelease(content)
        compare(activatedSpy.count, 1)
        compare(highlightedSpy.count, 1)
        tryCompare(control.popup, "visible", false)

        control.destroy()
    }

    function test_focus() {
        var control = comboBox.createObject(testCase, {model: 3})
        verify(control)

        waitForRendering(control)

        // click - gain focus - show popup
        mouseClick(control)
        verify(control.activeFocus)
        compare(control.popup.visible, true)

        // lose focus - hide popup
        control.focus = false
        verify(!control.activeFocus)
        tryCompare(control.popup, "visible", false)

        control.destroy()
    }

    function test_baseline() {
        var control = comboBox.createObject(testCase)
        verify(control)
        compare(control.baselineOffset, control.contentItem.y + control.contentItem.baselineOffset)
        control.destroy()
    }

    Component {
        id: displayBox
        ComboBox {
            textRole: "key"
            model: ListModel {
                ListElement { key: "First"; value: 123 }
                ListElement { key: "Second"; value: 456 }
                ListElement { key: "Third"; value: 789 }
            }
        }
    }

    function test_displayText() {
        var control = displayBox.createObject(testCase)
        verify(control)

        compare(control.displayText, "First")
        control.currentIndex = 1
        compare(control.displayText, "Second")
        control.textRole = "value"
        compare(control.displayText, "456")
        control.displayText = "Display"
        compare(control.displayText, "Display")
        control.currentIndex = 2
        compare(control.displayText, "Display")
        control.displayText = undefined
        compare(control.displayText, "789")

        control.destroy()
    }

    Component {
        id: component
        Pane {
            id: panel
            property alias button: _button;
            property alias combobox: _combobox;
            font.pixelSize: 30
            Column {
                Button {
                    id: _button
                    text: "Button"
                    font.pixelSize: 20
                }
                ComboBox {
                    id: _combobox
                    model: ["ComboBox", "With"]
                    delegate: ItemDelegate {
                        width: _combobox.width
                        text: _combobox.textRole ? (Array.isArray(_combobox.model) ? modelData[_combobox.textRole] : model[_combobox.textRole]) : modelData
                        objectName: "delegate"
                        autoExclusive: true
                        checked: _combobox.currentIndex === index
                        highlighted: _combobox.highlightedIndex === index
                    }
                }
            }
        }
    }

    function getChild(control, objname, idx) {
        var index = idx
        for (var i = index+1; i < control.children.length; i++)
        {
            if (control.children[i].objectName === objname) {
                index = i
                break
            }
        }
        return index
    }

    function test_font() { // QTBUG_50984, QTBUG-51696
        var control = component.createObject(testCase)
        verify(control)
        verify(control.button)
        verify(control.combobox)

        waitForRendering(control)

        compare(control.font.pixelSize, 30)
        compare(control.button.font.pixelSize, 20)
        compare(control.combobox.font.pixelSize, 30)

//        verify(control.combobox.popup)
//        var popup = control.combobox.popup
//        popup.open()

//        verify(popup.contentItem)

//        var listview = popup.contentItem
//        verify(listview.contentItem)
//        waitForRendering(listview)

//        var idx1 = getChild(listview.contentItem, "delegate", -1)
//        compare(listview.contentItem.children[idx1].font.pixelSize, 25)
//        var idx2 = getChild(listview.contentItem, "delegate", idx1)
//        compare(listview.contentItem.children[idx2].font.pixelSize, 25)

//        compare(listview.contentItem.children[idx1].font.pixelSize, 25)
//        compare(listview.contentItem.children[idx2].font.pixelSize, 25)

        control.font.pixelSize = control.font.pixelSize + 10
        compare(control.combobox.font.pixelSize, 40)
//        waitForRendering(listview)
//        compare(listview.contentItem.children[idx1].font.pixelSize, 25)
//        compare(listview.contentItem.children[idx2].font.pixelSize, 25)

        control.combobox.font.pixelSize = control.combobox.font.pixelSize + 5
        compare(control.combobox.font.pixelSize, 45)
//        waitForRendering(listview)

//        idx1 = getChild(listview.contentItem, "delegate", -1)
//        compare(listview.contentItem.children[idx1].font.pixelSize, 25)
//        idx2 = getChild(listview.contentItem, "delegate", idx1)
//        compare(listview.contentItem.children[idx2].font.pixelSize, 25)

        control.destroy()
    }

    function test_wheel() {
        var control = comboBox.createObject(testCase, {model: 2, wheelEnabled: true})
        verify(control)

        var delta = 120

        mouseWheel(control, control.width / 2, control.height / 2, -delta, -delta)
        compare(control.currentIndex, 1)

        // reached bounds -> no change
        mouseWheel(control, control.width / 2, control.height / 2, -delta, -delta)
        compare(control.currentIndex, 1)

        mouseWheel(control, control.width / 2, control.height / 2, delta, delta)
        compare(control.currentIndex, 0)

        // reached bounds -> no change
        mouseWheel(control, control.width / 2, control.height / 2, delta, delta)
        compare(control.currentIndex, 0)

        control.destroy()
    }

    function test_activation_data() {
        return [
            { tag: "open:enter", key: Qt.Key_Enter, open: true },
            { tag: "open:return", key: Qt.Key_Return, open: true },
            { tag: "closed:enter", key: Qt.Key_Enter, open: false },
            { tag: "closed:return", key: Qt.Key_Return, open: false }
        ]
    }

    // QTBUG-51645
    function test_activation(data) {
        var control = comboBox.createObject(testCase, {currentIndex: 1, model: ["Apple", "Orange", "Banana"]})
        verify(control)

        waitForRendering(control)
        control.forceActiveFocus()
        verify(control.activeFocus)

        if (data.open)
            keyClick(Qt.Key_Space)
        compare(control.popup.visible, data.open)

        compare(control.currentIndex, 1)
        compare(control.currentText, "Orange")
        compare(control.displayText, "Orange")

        keyClick(data.key)

        compare(control.currentIndex, 1)
        compare(control.currentText, "Orange")
        compare(control.displayText, "Orange")

        control.destroy()
    }

    Component {
        id: asyncLoader
        Loader {
            active: false
            asynchronous: true
            sourceComponent: ComboBox {
                model: ["First", "Second", "Third"]
            }
        }
    }

    // QTBUG-51972
    function test_async() {
        var loader = asyncLoader.createObject(testCase)
        verify(loader)

        loader.active = true
        tryCompare(loader, "status", Loader.Ready)
        verify(loader.item)
        compare(loader.item.currentText, "First")
        compare(loader.item.displayText, "First")

        loader.destroy()
    }

    // QTBUG-52615
    function test_currentIndex() {
        var control = comboBox.createObject(testCase, {currentIndex: -1, model: 3})
        verify(control)

        compare(control.currentIndex, -1)

        control.destroy()
    }

    ListModel {
        id: resetmodel
        ListElement { text: "First" }
        ListElement { text: "Second" }
        ListElement { text: "Third" }
    }

    // QTBUG-54573
    function test_modelReset() {
        var control = comboBox.createObject(testCase, {model: resetmodel})
        verify(control)
        control.popup.open()

        var listview = control.popup.contentItem
        verify(listview)

        waitForRendering(listview)
        compare(listview.contentItem.children.length, resetmodel.count + 1) // + highlight item

        resetmodel.clear()
        resetmodel.append({text: "Fourth"})
        resetmodel.append({text: "Fifth"})

        waitForRendering(listview)
        compare(listview.contentItem.children.length, resetmodel.count + 1) // + highlight item

        control.destroy()
    }

    // QTBUG-55118
    function test_currentText() {
        var control = comboBox.createObject(testCase, {model: listmodel})
        verify(control)

        compare(control.currentIndex, 0)
        compare(control.currentText, "First")

        listmodel.setProperty(0, "text", "1st")
        compare(control.currentText, "1st")

        control.currentIndex = 1
        compare(control.currentText, "Second")

        listmodel.setProperty(0, "text", "First")
        compare(control.currentText, "Second")

        control.destroy()
    }

    // QTBUG-55030
    function test_highlightRange() {
        var control = comboBox.createObject(testCase, {model: 100})
        verify(control)

        control.currentIndex = 50
        compare(control.currentIndex, 50)
        compare(control.highlightedIndex, -1)

        var openedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "opened"})
        verify(openedSpy.valid)

        control.popup.open()
        compare(control.highlightedIndex, 50)
        tryCompare(openedSpy, "count", 1)

        var listview = control.popup.contentItem
        verify(listview)

        var first = listview.itemAt(0, listview.contentY)
        verify(first)
        compare(first.text, "50")

        var closedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "closed"})
        verify(closedSpy.valid)

        control.popup.close()
        tryCompare(closedSpy, "count", 1)
        compare(control.highlightedIndex, -1)

        control.currentIndex = 99
        compare(control.currentIndex, 99)
        compare(control.highlightedIndex, -1)

        control.popup.open()
        compare(control.highlightedIndex, 99)
        tryCompare(openedSpy, "count", 2)

        var last = listview.itemAt(0, listview.contentY + listview.height - 1)
        verify(last)
        compare(last.text, "99")

        openedSpy.target = null
        closedSpy.target = null
        control.destroy()
    }
}
