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
import QtQuick.Window 2.2
import QtTest 1.0
import QtQuick.Controls 2.2

TestCase {
    id: testCase
    width: 400
    height: 400
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
                width: parent.width
            }
        }
    }

    function init() {
        // QTBUG-61225: Move the mouse away to avoid QQuickWindowPrivate::flushFrameSynchronousEvents()
        // delivering interfering hover events based on the last mouse position from earlier tests. For
        // example, ComboBox::test_activation() kept receiving hover events for the last mouse position
        // from CheckDelegate::test_checked().
        mouseMove(testCase, testCase.width - 1, testCase.height - 1)
    }

    function test_defaults() {
        var control = createTemporaryObject(comboBox, testCase)
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
    }

    function test_array() {
        var control = createTemporaryObject(comboBox, testCase)
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
    }

    function test_objects() {
        var control = createTemporaryObject(emptyBox, testCase)
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
    }

    function test_qobjects() {
        var control = createTemporaryObject(emptyBox, testCase, {textRole: "text"})
        verify(control)

        var obj1 = Qt.createQmlObject("import QtQml 2.0; QtObject { property string text: 'one' }", control)
        var obj2 = Qt.createQmlObject("import QtQml 2.0; QtObject { property string text: 'two' }", control)
        var obj3 = Qt.createQmlObject("import QtQml 2.0; QtObject { property string text: 'three' }", control)

        control.model = [obj1, obj2, obj3]

        compare(control.count, 3)
        compare(control.currentIndex, 0)
        compare(control.currentText, "one")

        control.currentIndex = 2
        compare(control.currentIndex, 2)
        compare(control.currentText, "three")

        control.model = null
        compare(control.model, null)
        compare(control.count, 0)
        compare(control.currentIndex, -1)
        compare(control.currentText, "")
    }

    function test_number() {
        var control = createTemporaryObject(comboBox, testCase)
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
        var control = createTemporaryObject(comboBox, testCase)
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
        var control = createTemporaryObject(emptyBox, testCase)
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
    }

    function test_textAt() {
        var control = createTemporaryObject(comboBox, testCase)
        verify(control)

        control.model = ["Apple", "Orange", "Banana"]
        compare(control.textAt(0), "Apple")
        compare(control.textAt(1), "Orange")
        compare(control.textAt(2), "Banana")
        compare(control.textAt(-1), "") // TODO: null?
        compare(control.textAt(5), "") // TODO: null?
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
        var control = createTemporaryObject(comboBox, testCase)
        verify(control)

        control.model = ["Banana", "banana", "Coconut", "Apple", "Cocomuffin"]

        compare(control.find(data.term, data.flags), data.index)
    }


    function test_arrowKeys() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
        verify(control)

        var activatedSpy = signalSpy.createObject(control, {target: control, signalName: "activated"})
        verify(activatedSpy.valid)

        var highlightedSpy = signalSpy.createObject(control, {target: control, signalName: "highlighted"})
        verify(highlightedSpy.valid)

        var openedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "opened"})
        verify(openedSpy.valid)

        var closedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "closed"})
        verify(closedSpy.valid)

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
        openedSpy.wait()
        compare(openedSpy.count, 1)

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
        closedSpy.wait()
        compare(closedSpy.count, 1)

        compare(control.currentIndex, 1)
        compare(control.highlightedIndex, -1)
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
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
        verify(control)

        var openedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "opened"})
        verify(openedSpy.valid)

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
        if (data.showPopup)
            openedSpy.wait()

        // hide popup
        keyPress(data.key2)
        compare(control.pressed, data.hidePress)
        keyRelease(data.key2)
        compare(control.pressed, false)
        tryCompare(control.popup, "visible", !data.hidePopup)
    }

    function test_keys_home_end() {
        var control = createTemporaryObject(comboBox, testCase, {model: 5})
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
    }

    function test_keySearch() {
        var control = createTemporaryObject(comboBox, testCase, {model: ["Banana", "Coco", "Coconut", "Apple", "Cocomuffin"]})
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
    }

    function test_popup() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
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
    }

    function test_mouse() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
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
    }

    function test_touch() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
        verify(control)

        var touch = touchEvent(control)

        var activatedSpy = signalSpy.createObject(control, {target: control, signalName: "activated"})
        verify(activatedSpy.valid)

        var highlightedSpy = signalSpy.createObject(control, {target: control, signalName: "highlighted"})
        verify(highlightedSpy.valid)

        touch.press(0, control).commit()
        touch.release(0, control).commit()
        compare(control.popup.visible, true)

        var content = control.popup.contentItem
        waitForRendering(content)

        // press - move - release outside - not activated - not closed
        touch.press(0, control).commit()
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        touch.move(0, control, control.width * 2, control.height / 2).commit()
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        touch.release(0, control, control.width * 2, control.height / 2).commit()
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        compare(control.popup.visible, true)

        // press - move - release inside - activated - closed
        touch.press(0, content).commit()
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        touch.move(0, content, content.width / 2 + 1, content.height / 2 + 1).commit()
        compare(activatedSpy.count, 0)
        compare(highlightedSpy.count, 0)
        touch.release(0, content).commit()
        compare(activatedSpy.count, 1)
        compare(highlightedSpy.count, 1)
        tryCompare(control.popup, "visible", false)
    }

    function test_down() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
        verify(control)

        // some styles position the popup over the combo button. move it out
        // of the way to avoid stealing mouse presses. we want to test the
        // combinations of the button being pressed and the popup being visible.
        control.popup.y = control.height

        var downSpy = signalSpy.createObject(control, {target: control, signalName: "downChanged"})
        verify(downSpy.valid)

        var pressedSpy = signalSpy.createObject(control, {target: control, signalName: "pressedChanged"})
        verify(pressedSpy.valid)

        mousePress(control)
        compare(control.popup.visible, false)
        compare(control.pressed, true)
        compare(control.down, true)
        compare(downSpy.count, 1)
        compare(pressedSpy.count, 1)

        mouseRelease(control)
        compare(control.popup.visible, true)
        compare(control.pressed, false)
        compare(control.down, true)
        compare(downSpy.count, 3)
        compare(pressedSpy.count, 2)

        compare(control.popup.y, control.height)

        control.down = false
        compare(control.down, false)
        compare(downSpy.count, 4)

        mousePress(control)
        compare(control.popup.visible, true)
        compare(control.pressed, true)
        compare(control.down, false) // explicit false
        compare(downSpy.count, 4)
        compare(pressedSpy.count, 3)

        control.down = undefined
        compare(control.down, true)
        compare(downSpy.count, 5)

        mouseRelease(control)
        tryCompare(control.popup, "visible", false)
        compare(control.pressed, false)
        compare(control.down, false)
        compare(downSpy.count, 6)
        compare(pressedSpy.count, 4)

        control.popup.open()
        compare(control.popup.visible, true)
        compare(control.pressed, false)
        compare(control.down, true)
        compare(downSpy.count, 7)
        compare(pressedSpy.count, 4)

        control.popup.close()
        tryCompare(control.popup, "visible", false)
        compare(control.pressed, false)
        compare(control.down, false)
        compare(downSpy.count, 8)
        compare(pressedSpy.count, 4)
    }

    function test_focus() {
        var control = createTemporaryObject(comboBox, testCase, {model: 3})
        verify(control)

        var openedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "opened"})
        verify(openedSpy.valid)

        var closedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "closed"})
        verify(openedSpy.valid)

        // click - gain focus - show popup
        mouseClick(control)
        verify(control.activeFocus)
        openedSpy.wait()
        compare(openedSpy.count, 1)
        compare(control.popup.visible, true)

        // lose focus - hide popup
        control.focus = false
        verify(!control.activeFocus)
        closedSpy.wait()
        compare(closedSpy.count, 1)
        compare(control.popup.visible, false)
    }

    function test_baseline() {
        var control = createTemporaryObject(comboBox, testCase)
        verify(control)
        compare(control.baselineOffset, control.contentItem.y + control.contentItem.baselineOffset)
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
        var control = createTemporaryObject(displayBox, testCase)
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
        var control = createTemporaryObject(component, testCase)
        verify(control)
        verify(control.button)
        verify(control.combobox)

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
    }

    function test_wheel() {
        var control = createTemporaryObject(comboBox, testCase, {model: 2, wheelEnabled: true})
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
        var control = createTemporaryObject(comboBox, testCase, {currentIndex: 1, model: ["Apple", "Orange", "Banana"]})
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        if (data.open) {
            var openedSpy = signalSpy.createObject(control, {target: control.popup, signalName: "opened"})
            verify(openedSpy.valid)

            keyClick(Qt.Key_Space)
            openedSpy.wait()
            compare(openedSpy.count, 1)
        }
        compare(control.popup.visible, data.open)

        compare(control.currentIndex, 1)
        compare(control.currentText, "Orange")
        compare(control.displayText, "Orange")

        keyClick(data.key)

        compare(control.currentIndex, 1)
        compare(control.currentText, "Orange")
        compare(control.displayText, "Orange")
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
        var loader = createTemporaryObject(asyncLoader, testCase)
        verify(loader)

        loader.active = true
        tryCompare(loader, "status", Loader.Ready)
        verify(loader.item)
        compare(loader.item.currentText, "First")
        compare(loader.item.displayText, "First")
    }

    // QTBUG-52615
    function test_currentIndex() {
        var control = createTemporaryObject(comboBox, testCase, {currentIndex: -1, model: 3})
        verify(control)

        compare(control.currentIndex, -1)
    }

    ListModel {
        id: resetmodel
        ListElement { text: "First" }
        ListElement { text: "Second" }
        ListElement { text: "Third" }
    }

    // QTBUG-54573
    function test_modelReset() {
        var control = createTemporaryObject(comboBox, testCase, {model: resetmodel})
        verify(control)
        control.popup.open()

        var listview = control.popup.contentItem
        verify(listview)

        tryCompare(listview.contentItem.children, "length", resetmodel.count + 1) // + highlight item

        resetmodel.clear()
        resetmodel.append({text: "Fourth"})
        resetmodel.append({text: "Fifth"})

        tryCompare(listview.contentItem.children, "length", resetmodel.count + 1) // + highlight item
    }

    // QTBUG-55118
    function test_currentText() {
        var control = createTemporaryObject(comboBox, testCase, {model: listmodel})
        verify(control)

        compare(control.currentIndex, 0)
        compare(control.currentText, "First")

        listmodel.setProperty(0, "text", "1st")
        compare(control.currentText, "1st")

        control.currentIndex = 1
        compare(control.currentText, "Second")

        listmodel.setProperty(0, "text", "First")
        compare(control.currentText, "Second")
    }

    // QTBUG-55030
    function test_highlightRange() {
        var control = createTemporaryObject(comboBox, testCase, {model: 100})
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
        tryVerify(function() { return listview.height > 0 })

        var last = listview.itemAt(0, listview.contentY + listview.height - 1)
        verify(last)
        compare(last.text, "99")

        openedSpy.target = null
        closedSpy.target = null
    }

    RegExpValidator {
        id: regExpValidator
        regExp: /(red|blue|green)?/
    }

    function test_validator() {
        var control = createTemporaryObject(comboBox, testCase, {editable: true, validator: regExpValidator})

        control.editText = "blu"
        compare(control.acceptableInput, false)
        control.editText = "blue"
        compare(control.acceptableInput, true)
        control.editText = "bluee"
        compare(control.acceptableInput, false)
        control.editText = ""
        compare(control.acceptableInput, true)
        control.editText = ""
        control.forceActiveFocus()
        keyPress(Qt.Key_A)
        compare(control.editText, "")
        keyPress(Qt.Key_A)
        compare(control.editText, "")
        keyPress(Qt.Key_R)
        compare(control.editText, "r")
        keyPress(Qt.Key_A)
        compare(control.editText, "r")
        compare(control.acceptableInput, false)
        keyPress(Qt.Key_E)
        compare(control.editText, "re")
        compare(control.acceptableInput, false)
        keyPress(Qt.Key_D)
        compare(control.editText, "red")
        compare(control.acceptableInput, true)
    }

    Component {
        id: appendFindBox
        ComboBox {
            editable: true
            model: ListModel {
                ListElement { text:"first" }
            }
            onAccepted: {
                if (find(editText) === -1)
                    model.append({text: editText})
            }
        }
    }

    function test_append_find() {
        var control = createTemporaryObject(appendFindBox, testCase)

        compare(control.currentIndex, 0)
        compare(control.currentText, "first")
        control.forceActiveFocus()
        compare(control.activeFocus, true)

        control.selectAll()
        keyPress(Qt.Key_T)
        keyPress(Qt.Key_H)
        keyPress(Qt.Key_I)
        keyPress(Qt.Key_R)
        keyPress(Qt.Key_D)
        compare(control.count, 1)
        compare(control.currentText, "first")
        compare(control.editText, "third")

        keyPress(Qt.Key_Enter)
        compare(control.count, 2)
        compare(control.currentIndex, 1)
        compare(control.currentText, "third")
    }

    function test_editable() {
        var control = createTemporaryObject(comboBox, testCase, {editable: true, model: ["Banana", "Coco", "Coconut", "Apple", "Cocomuffin"]})
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        var acceptCount = 0

        var acceptSpy = signalSpy.createObject(control, {target: control, signalName: "accepted"})
        verify(acceptSpy.valid)

        compare(control.editText, "Banana")
        compare(control.currentText, "Banana")
        compare(control.currentIndex, 0)
        compare(acceptSpy.count, 0)
        control.editText = ""

        keyPress(Qt.Key_C)
        compare(control.editText, "coco")
        compare(control.currentText, "Banana")
        compare(control.currentIndex, 0)

        keyPress(Qt.Key_Right)
        keyPress(Qt.Key_N)
        compare(control.editText, "coconut")
        compare(control.currentText, "Banana")
        compare(control.currentIndex, 0)

        keyPress(Qt.Key_Enter) // Accept
        compare(control.editText, "Coconut")
        compare(control.currentText, "Coconut")
        compare(control.currentIndex, 2)
        compare(acceptSpy.count, ++acceptCount)

        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_M)
        compare(control.editText, "Cocomuffin")
        compare(control.currentText, "Coconut")
        compare(control.currentIndex, 2)

        keyPress(Qt.Key_Enter) // Accept
        compare(control.editText, "Cocomuffin")
        compare(control.currentText, "Cocomuffin")
        compare(control.currentIndex, 4)
        compare(acceptSpy.count, ++acceptCount)

        keyPress(Qt.Key_Return) // Accept
        compare(control.editText, "Cocomuffin")
        compare(control.currentText, "Cocomuffin")
        compare(control.currentIndex, 4)
        compare(acceptSpy.count, ++acceptCount)

        control.editText = ""
        compare(control.editText, "")
        compare(control.currentText, "Cocomuffin")
        compare(control.currentIndex, 4)

        keyPress(Qt.Key_A)
        compare(control.editText, "apple")
        compare(control.currentText, "Cocomuffin")
        compare(control.currentIndex, 4)

        keyPress(Qt.Key_Return) // Accept
        compare(control.editText, "Apple")
        compare(control.currentText, "Apple")
        compare(control.currentIndex, 3)
        compare(acceptSpy.count, ++acceptCount)

        control.editText = ""
        keyPress(Qt.Key_A)
        keyPress(Qt.Key_B)
        compare(control.editText, "ab")
        compare(control.currentText, "Apple")
        compare(control.currentIndex, 3)

        keyPress(Qt.Key_Return) // Accept
        compare(control.editText, "ab")
        compare(control.currentText, "")
        compare(control.currentIndex, -1)
        compare(acceptSpy.count, ++acceptCount)

        control.editText = ""
        compare(control.editText, "")
        compare(control.currentText, "")
        compare(control.currentIndex, -1)

        keyPress(Qt.Key_C)
        keyPress(Qt.Key_Return) // Accept
        compare(control.editText, "Coco")
        compare(control.currentText, "Coco")
        compare(control.currentIndex, 1)
        compare(acceptSpy.count, ++acceptCount)

        keyPress(Qt.Key_Down)
        compare(control.editText, "Coconut")
        compare(control.currentText, "Coconut")
        compare(control.currentIndex, 2)

        keyPress(Qt.Key_Up)
        compare(control.editText, "Coco")
        compare(control.currentText, "Coco")
        compare(control.currentIndex, 1)

        control.editText = ""
        compare(control.editText, "")
        compare(control.currentText, "Coco")
        compare(control.currentIndex, 1)

        keyPress(Qt.Key_C)
        keyPress(Qt.Key_O)
        keyPress(Qt.Key_C) // autocompletes "coco"
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Return) // Accept "coc"
        compare(control.editText, "coc")
        compare(control.currentText, "")
        compare(control.currentIndex, -1)
        compare(acceptSpy.count, ++acceptCount)

        control.editText = ""
        compare(control.editText, "")
        compare(control.currentText, "")
        compare(control.currentIndex, -1)

        keyPress(Qt.Key_C)
        keyPress(Qt.Key_O)
        keyPress(Qt.Key_C) // autocompletes "coc"
        keyPress(Qt.Key_Space)
        keyPress(Qt.Key_Return) // Accept "coc "
        compare(control.editText, "coc ")
        compare(control.currentText, "")
        compare(control.currentIndex, -1)
        compare(acceptSpy.count, ++acceptCount)
    }

    Component {
        id: keysAttachedBox
        ComboBox {
            editable: true
            property bool gotit: false
            Keys.onPressed: {
                if (!gotit && event.key === Qt.Key_B) {
                    gotit = true
                    event.accepted = true
                }
            }
        }
    }

    function test_keys_attached() {
        var control = createTemporaryObject(keysAttachedBox, testCase)
        verify(control)

        control.forceActiveFocus()
        verify(control.activeFocus)

        verify(!control.gotit)
        compare(control.editText, "")

        keyPress(Qt.Key_A)
        verify(control.activeFocus)
        verify(!control.gotit)
        compare(control.editText, "a")

        keyPress(Qt.Key_B)
        verify(control.activeFocus)
        expectFail("", "An editable ComboBox does not yet support the Keys attached property.")
        verify(control.gotit)
        compare(control.editText, "a")

        keyPress(Qt.Key_B)
        verify(control.activeFocus)
        verify(control.gotit)
        compare(control.editText, "ab")
    }

    function test_minusOneIndexResetsSelection_QTBUG_35794_data() {
        return [
            { tag: "editable", editable: true },
            { tag: "non-editable", editable: false }
        ]
    }

    function test_minusOneIndexResetsSelection_QTBUG_35794(data) {
        var control = createTemporaryObject(comboBox, testCase, {editable: data.editable, model: ["A", "B", "C"]})
        verify(control)

        compare(control.currentIndex, 0)
        compare(control.currentText, "A")
        control.currentIndex = -1
        compare(control.currentIndex, -1)
        compare(control.currentText, "")
        control.currentIndex = 1
        compare(control.currentIndex, 1)
        compare(control.currentText, "B")
    }

    function test_minusOneToZeroSelection_QTBUG_38036() {
        var control = createTemporaryObject(comboBox, testCase, {model: ["A", "B", "C"]})
        verify(control)

        compare(control.currentIndex, 0)
        compare(control.currentText, "A")
        control.currentIndex = -1
        compare(control.currentIndex, -1)
        compare(control.currentText, "")
        control.currentIndex = 0
        compare(control.currentIndex, 0)
        compare(control.currentText, "A")
    }

    function test_emptyPopupAfterModelCleared() {
        var control = createTemporaryObject(comboBox, testCase, { model: 1 })
        verify(control)
        compare(control.popup.implicitHeight, 0)
        compare(control.popup.height, 0)

        // Ensure that it's open so that the popup's implicitHeight changes when we increase the model count.
        control.popup.open()
        tryCompare(control.popup, "visible", true)

        // Add lots of items to the model. The popup should take up the entire height of the window.
        control.model = 100
        compare(control.popup.height, control.Window.height - control.popup.topMargin - control.popup.bottomMargin)

        control.popup.close()

        // Clearing the model should result in a zero height.
        control.model = 0
        control.popup.open()
        tryCompare(control.popup, "visible", true)
        compare(control.popup.height, 0)
    }
}
