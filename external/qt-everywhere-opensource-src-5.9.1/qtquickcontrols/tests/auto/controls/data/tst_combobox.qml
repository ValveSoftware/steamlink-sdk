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
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 400
    height: 400

TestCase {
    id: testCase
    name: "Tests_ComboBox"
    when:windowShown
    width:400
    height:400

    property var model

    Timer {
        id: timer
        running: true
        repeat: false
        interval: 500
        onTriggered: testCase.keyPress(Qt.Key_Escape)
    }

    function init() {
        model = Qt.createQmlObject("import QtQuick 2.2; ListModel {}", testCase, '')
        model.append({ text: "Banana", color: "Yellow" })
        model.append({ text: "Apple", color: "Green" })
        model.append({ text: "Coconut", color: "Brown" })
    }

    function cleanup() {
        if (model !== 0)
            model.destroy()
        wait(0) // spin the event loop to get all popups to close
    }

    function test_keyupdown() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: 4 }', testCase, '');

        compare(comboBox.currentIndex, 0)

        comboBox.forceActiveFocus()

        keyPress(Qt.Key_Down)
        compare(comboBox.currentIndex, 1)
        keyPress(Qt.Key_Down)
        compare(comboBox.currentIndex, 2)
        keyPress(Qt.Key_Up)
        compare(comboBox.currentIndex, 1)
        comboBox.destroy()
    }

    function test_textrole() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.textRole = "text"
        comboBox.model = model
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.textRole = "color"
        compare(comboBox.currentText, "Yellow")
        comboBox.destroy()
    }

    function test_arraymodel() {
        var arrayModel = [
            'Banana',
            'Apple',
            'Coconut'
        ];

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.destroy()
    }

    function test_arraymodelwithtextrole() {
        var arrayModel = [
            {text: 'Banana', color: 'Yellow'},
            {text: 'Apple', color: 'Green'},
            {text: 'Coconut', color: 'Brown'}
        ];

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { }', testCase, '');
        comboBox.textRole = "text"
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.textRole = "color"
        compare(comboBox.currentText, "Yellow")
        comboBox.destroy()
    }

    function test_arrayModelWithoutTextRole() {
        var arrayModel = ['Banana', 'Coconut', 'Apple']

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.destroy()

        comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.editable = true
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.destroy()
    }

    function test_validator() {
        var comboBox = Qt.createQmlObject('import QtQuick 2.2;              \
                                            import QtQuick.Controls 1.2;     \
                                            ComboBox {                       \
                                                editable: true;              \
                                                validator: RegExpValidator { \
                                                                regExp: /(red|blue|green)?/ \
                                            }}', testCase, '')

        comboBox.editText = "blu"
        compare(comboBox.acceptableInput, false)
        comboBox.editText = "blue"
        compare(comboBox.acceptableInput, true)
        comboBox.editText = "bluee"
        compare(comboBox.acceptableInput, false)
        comboBox.editText = ""
        compare(comboBox.acceptableInput, true)
        comboBox.editText = ""
        comboBox.forceActiveFocus()
        keyPress(Qt.Key_A)
        compare(comboBox.editText, "")
        keyPress(Qt.Key_A)
        compare(comboBox.editText, "")
        keyPress(Qt.Key_R)
        compare(comboBox.editText, "r")
        keyPress(Qt.Key_A)
        compare(comboBox.editText, "r")
        compare(comboBox.acceptableInput, false)
        keyPress(Qt.Key_E)
        compare(comboBox.editText, "re")
        compare(comboBox.acceptableInput, false)
        keyPress(Qt.Key_D)
        compare(comboBox.editText, "red")
        compare(comboBox.acceptableInput, true)
        comboBox.destroy()
    }

    function test_append_find() {
    var comboBox = Qt.createQmlObject( 'import QtQuick.Controls 1.2;                    \
                                        import QtQuick 2.2;                             \
                                        ComboBox {                                      \
                                            model:ListModel{ListElement{text:"first"}}  \
                                            onAccepted: {                               \
                                            if (find(currentText) === -1) {             \
                                                model.append({text: editText});         \
                                                currentIndex = find(editText);          \
                                            }                                           \
                                        }                                               \
                                       }', testCase, '')

        comboBox.editable = true
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "first")
        comboBox.forceActiveFocus();

        comboBox.selectAll();
        keyPress(Qt.Key_T)
        keyPress(Qt.Key_H)
        keyPress(Qt.Key_I)
        keyPress(Qt.Key_R)
        keyPress(Qt.Key_D)
        compare(comboBox.count, 1)
        compare(comboBox.currentText, "first")
        compare(comboBox.editText, "third")

        keyPress(Qt.Key_Enter)
        compare(comboBox.count, 2)
        compare(comboBox.currentIndex, 1)
        compare(comboBox.currentText, "third")
        comboBox.destroy()
    }

    function test_editable() {
        var arrayModel = ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2;         \
                                            ComboBox {                          \
                                                property int acceptedCount: 0;  \
                                                onAccepted: ++acceptedCount;    \
                                           }'
                                          , testCase, '');
        comboBox.model = arrayModel
        comboBox.editable = true
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        compare(comboBox.acceptedCount, 0)
        comboBox.forceActiveFocus()
        comboBox.editText = ""

        keyPress(Qt.Key_C)
        compare(comboBox.currentText, "Banana")
        compare(comboBox.editText, "coco")
        compare(comboBox.currentIndex, 0)

        keyPress(Qt.Key_Right)
        keyPress(Qt.Key_N)
        compare(comboBox.currentText, "Banana")
        compare(comboBox.editText, "coconut")
        keyPress(Qt.Key_Enter) // Accept

        compare(comboBox.currentText, "Coconut")
        compare(comboBox.currentIndex, 2)

        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_M)
        compare(comboBox.currentText, "Coconut")
        compare(comboBox.editText, "Cocomuffin")
        keyPress(Qt.Key_Enter) // Accept

        compare(comboBox.currentText, "Cocomuffin")
        compare(comboBox.currentIndex, 4)
        keyPress(Qt.Key_Return) // Accept
        compare(comboBox.acceptedCount, 3)

        comboBox.editText = ""
        compare(comboBox.editText, "")

        keyPress(Qt.Key_A)
        compare(comboBox.currentText, "Cocomuffin")
        keyPress(Qt.Key_Return) // Accept

        compare(comboBox.currentText, "Apple")
        compare(comboBox.editText, "Apple")
        compare(comboBox.currentIndex, 3)

        comboBox.editText = ""
        keyPress(Qt.Key_A)
        keyPress(Qt.Key_B)
        compare(comboBox.currentText, "Apple")
        compare(comboBox.editText, "ab")
        compare(comboBox.currentIndex, 3)

        keyPress(Qt.Key_Return) // Accept
        compare(comboBox.currentText, "ab")
        compare(comboBox.currentIndex, -1)

        // Test up down
        comboBox.editText = ""

        keyPress(Qt.Key_C)
        compare(comboBox.currentText, "ab")
        keyPress(Qt.Key_Return) // Accept
        compare(comboBox.currentText, "Coco")
        compare(comboBox.editText, "Coco")
        compare(comboBox.currentIndex, 1)

        keyPress(Qt.Key_Down)
        compare(comboBox.currentText, "Coconut")
        compare(comboBox.editText, "Coconut")
        compare(comboBox.currentIndex, 2)

        keyPress(Qt.Key_Up)
        compare(comboBox.currentText, "Coco")
        compare(comboBox.editText, "Coco")
        compare(comboBox.currentIndex, 1)

        comboBox.editText = ""
        keyPress(Qt.Key_C)
        keyPress(Qt.Key_O)
        keyPress(Qt.Key_C) // autocompletes "coco"
        keyPress(Qt.Key_Backspace)
        keyPress(Qt.Key_Return) // Accept "coc"
        compare(comboBox.editText, "coc")
        compare(comboBox.currentText, "coc")

        comboBox.editText = ""
        keyPress(Qt.Key_C)
        keyPress(Qt.Key_O)
        keyPress(Qt.Key_C) // autocompletes "coc"
        keyPress(Qt.Key_Space)
        keyPress(Qt.Key_Return) // Accept "coc "
        compare(comboBox.editText, "coc ")
        compare(comboBox.currentText, "coc ")

        comboBox.destroy()
    }

    function test_keySearch() {
        var arrayModel = ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        comboBox.forceActiveFocus()

        keyPress(Qt.Key_C)
        compare(comboBox.editText, "Coco")
        compare(comboBox.currentText, "Coco")
        compare(comboBox.currentIndex, 1)

        keyPress(Qt.Key_N)
        compare(comboBox.editText, "Coco")
        compare(comboBox.currentText, "Coco")
        compare(comboBox.currentIndex, 1)

        keyPress(Qt.Key_A)
        compare(comboBox.editText, "Apple")
        compare(comboBox.currentText, "Apple")
        compare(comboBox.currentIndex, 3)

        keyPress(Qt.Key_B)
        compare(comboBox.editText, "Banana")
        compare(comboBox.currentText, "Banana")
        compare(comboBox.currentIndex, 0)

        comboBox.destroy()
    }

    function test_textAt() {
        var arrayModel = ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        compare(comboBox.textAt(5), null)
        compare(comboBox.textAt(-1), null)
        compare(comboBox.textAt(0), "Banana")
        compare(comboBox.textAt(1), "Coco")
        compare(comboBox.textAt(2), "Coconut")
        compare(comboBox.textAt(3), "Apple")
        compare(comboBox.textAt(4), "Cocomuffin")
        comboBox.destroy()
    }

    function test_find() {
        var arrayModel = ['Banana', 'banana', 'Coconut', 'Apple', 'Cocomuffin' ]
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        compare(comboBox.find("Banana"), 0)
        compare(comboBox.find("banana"), 1)
        compare(comboBox.find("bananas"), -1)
        compare(comboBox.find("Apple"), 3)
        compare(comboBox.find("Cocomuffin"), 4)
        comboBox.destroy()
    }

    function test_activated() {
        var arrayModel = ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2;         \
                                            ComboBox {                          \
                                                property int activatedCount: 0;  \
                                                onActivated: ++activatedCount;    \
                                           }'
                                          , testCase, '');
        comboBox.model = arrayModel
        compare(comboBox.currentIndex, 0)
        comboBox.forceActiveFocus()

        keyPress(Qt.Key_Down)
        compare(comboBox.activatedCount, 1)
        keyPress(Qt.Key_Down)
        compare(comboBox.activatedCount, 2)
        keyPress(Qt.Key_Up)
        compare(comboBox.activatedCount, 3)
        keyPress(Qt.Key_B)
        compare(comboBox.activatedCount, 4)
        keyPress(Qt.Key_B)
        compare(comboBox.activatedCount, 4)
        comboBox.destroy()
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
            ComboBox  {                         \
                y: 20;                          \
                id: _control1;                  \
                activeFocusOnTab: true;         \
            }                                   \
            ComboBox  {                         \
                y: 70;                          \
                id: _control2;                  \
                activeFocusOnTab: false;        \
            }                                   \
            ComboBox  {                         \
                y: 120;                         \
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

    function test_activeFocusOnPress(){
        if (Qt.platform.os === "osx")
            skip("When the menu pops up on OS X, it does not return and the test fails after time out")

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: 4 }', container, '');
        comboBox.activeFocusOnPress = false
        verify(!comboBox.activeFocus)
        if (Qt.platform.os === "osx") // on mac when the menu open, the __popup function does not return
            timer.start()
        else // two mouse clicks to open and close the popup menu
            mouseClick(comboBox, comboBox.x + 1, comboBox.y + 1)
        mouseClick(comboBox, comboBox.x + 1, comboBox.y + 1)
        verify(!comboBox.activeFocus)
        comboBox.activeFocusOnPress = true
        if (Qt.platform.os === "osx") // on mac when the menu open, the __popup function does not return
            timer.start()
        else // two mouse clicks to open and close the popup menu
            mouseClick(comboBox, comboBox.x + 1, comboBox.y + 1)
        mouseClick(comboBox, comboBox.x + 1, comboBox.y + 1)
        verify(comboBox.activeFocus)
        comboBox.destroy()
    }

    function test_spaceKey(){
        if (Qt.platform.os === "osx")
            skip("When the menu pops up on OS X, it does not return and the test fails after time out")

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: 4 }', container, '');
        var menuIndex = getMenuIndex(comboBox)
        verify(menuIndex !== -1)
        comboBox.forceActiveFocus()
        verify(!comboBox.data[menuIndex].__popupVisible)
        keyPress(Qt.Key_Space)
        verify(comboBox.data[menuIndex].__popupVisible)

        // close the menu before destroying the combobox
        comboBox.data[menuIndex].__closeAndDestroy()
        verify(!comboBox.data[menuIndex].__popupVisible)

        comboBox.destroy()
    }

    function test_currentIndexInMenu() {
        if (Qt.platform.os === "osx")
            skip("When the menu pops up on OS X, it does not return and the test fails after time out")

        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: 4 }', container, '');
        var menuIndex = getMenuIndex(comboBox)
        comboBox.currentIndex = 2
        verify(menuIndex !== -1)
        comboBox.forceActiveFocus()
        keyPress(Qt.Key_Space)
        verify(comboBox.data[menuIndex].__popupVisible)
        compare(comboBox.data[menuIndex].__currentIndex, comboBox.currentIndex)
        for (var i = 0; i < comboBox.data[menuIndex].items.length; i++)
        {
            if ( i !== comboBox.currentIndex)
                verify(!comboBox.data[menuIndex].items[i].checked)
            else
                verify(comboBox.data[menuIndex].items[i].checked)
        }
        // close the menu before destroying the combobox
        comboBox.data[menuIndex].__closeAndDestroy()
        verify(!comboBox.data[menuIndex].__popupVisible)
        comboBox.destroy()
    }

    SignalSpy {
        id: modelSpy
        signalName: "modelChanged"
    }

    SignalSpy {
        id: textSpy
        signalName: "currentTextChanged"
    }

    SignalSpy {
        id: indexSpy
        signalName: "currentIndexChanged"
    }

    function test_modelChange() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: ["a", "b", "c", "d"] }', container, '');
        modelSpy.target = textSpy.target = indexSpy.target = comboBox

        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "a")

        // 1st model change
        comboBox.model = ["A", "B", "C", "D"]
        compare(comboBox.currentIndex, 0)
        compare(modelSpy.count, 1)
        compare(indexSpy.count, 0)
        compare(textSpy.count, 1)
        modelSpy.clear()
        indexSpy.clear()
        textSpy.clear()

        // Setting currentIndex
        comboBox.currentIndex = 3
        compare(indexSpy.count, 1)
        compare(textSpy.count, 1)
        indexSpy.clear()
        textSpy.clear()

        // 2nd model change
        comboBox.model = 4
        compare(comboBox.currentIndex, 0)
        compare(modelSpy.count, 1)
        compare(indexSpy.count, 1)
        compare(textSpy.count, 1)
        modelSpy.clear()
        indexSpy.clear()
        textSpy.clear()

        // 3rd model change
        comboBox.model = ["a", "b", "c", "d"]
        compare(comboBox.currentIndex, 0)
        compare(modelSpy.count, 1)
        compare(indexSpy.count, 0)
        compare(textSpy.count, 1)

        comboBox.destroy()
    }

    function test_addRemoveItemsInModel_QTBUG_30379() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.textRole = "text"
        comboBox.model = model
        var menuIndex = getMenuIndex(comboBox)
        verify(menuIndex !== -1)
        compare(comboBox.model.count, 3)
        compare(comboBox.data[menuIndex].items.length, 3)
        comboBox.model.append({ text: "Tomato", color: "Red" })
        compare(comboBox.model.count, 4)
        compare(comboBox.data[menuIndex].items.length, 4)
        comboBox.model.remove(2, 2)
        compare(comboBox.model.count, 2)
        compare(comboBox.data[menuIndex].items.length, 2)
        comboBox.destroy()
    }

    function test_width_QTBUG_30377() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: ["A", "BB", "CCCCC"] }', testCase, '');
        compare(comboBox.currentIndex, 0)
        var initialWidth = comboBox.width
        comboBox.currentIndex = 1
        compare(comboBox.width, initialWidth)
        comboBox.currentIndex = 2
        compare(comboBox.width, initialWidth)
        comboBox.destroy()
    }

    function test_stringListModel() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { }', testCase, '');
        comboBox.model = model_qstringlist;
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "A")
        comboBox.currentIndex = 1
        compare(comboBox.currentIndex, 1)
        compare(comboBox.currentText, "B")
        comboBox.currentIndex = 2
        compare(comboBox.currentIndex, 2)
        compare(comboBox.currentText, "C")
        comboBox.destroy()
    }

    function test_variantListModel() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { }', testCase, '');
        comboBox.model = model_qvarlist;
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "3")
        comboBox.currentIndex = 1
        compare(comboBox.currentIndex, 1)
        compare(comboBox.currentText, "2")
        comboBox.currentIndex = 2
        compare(comboBox.currentIndex, 2)
        compare(comboBox.currentText, "1")
        comboBox.destroy()
    }

    function getMenuIndex(control) {
        var index = -1
        for (var i = 0; i < control.data.length; i++)
        {
            if (control.data[i].objectName === 'popup') {
                index = i
                break
            }
        }
        return index
    }

    function test_emptyTextItem() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { }', testCase, '');
        comboBox.model = [
            "1",
            "",
            "3"
        ]
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "1")
        comboBox.currentIndex = 1
        compare(comboBox.currentIndex, 1)
        compare(comboBox.currentText, "")
        comboBox.currentIndex = 2
        compare(comboBox.currentIndex, 2)
        compare(comboBox.currentText, "3")
        compare(comboBox.find(""), 1)
        comboBox.destroy()
    }

    function test_minusOneIndexResetsSelection_QTBUG_35794() {
        var qmlObjects = ['import QtQuick.Controls 1.2 ; ComboBox { model: ["A", "B", "C"] }',
                          'import QtQuick.Controls 1.2 ; ComboBox { editable: true; model: ["A", "B", "C"] }']
        for (var i = 0; i < qmlObjects.length; i++) {
            var comboBox = Qt.createQmlObject(qmlObjects[i], testCase, '');
            compare(comboBox.currentIndex, 0)
            compare(comboBox.currentText, "A")
            comboBox.currentIndex = -1
            compare(comboBox.currentIndex, -1)
            compare(comboBox.currentText, "")
            comboBox.currentIndex = 1
            compare(comboBox.currentIndex, 1)
            compare(comboBox.currentText, "B")
            comboBox.destroy()
        }
    }

    function test_minusOneToZeroSelection_QTBUG_38036() {
        var qmlObject = 'import QtQuick.Controls 1.2 ; ComboBox { model: ["A", "B", "C"] }'
        var comboBox = Qt.createQmlObject(qmlObject, testCase, '');
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "A")
        comboBox.currentIndex = -1
        compare(comboBox.currentIndex, -1)
        compare(comboBox.currentText, "")
        comboBox.currentIndex = 0
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "A")
        comboBox.destroy()
    }

    function test_keys() {
        var component = Qt.createComponent("combobox/cb_keys.qml")
        compare(component.status, Component.Ready)
        var test =  component.createObject(container);
        verify(test !== null, "test control created is null")
        var control1 = test.control1
        verify(control1 !== null)

        control1.forceActiveFocus()
        verify(control1.activeFocus)

        verify(control1.gotit === false)
        verify(control1.editText === "0")

        keyPress(Qt.Key_A)
        verify(control1.activeFocus)
        verify(control1.gotit === false)
        verify(control1.editText === "0a")

        keyPress(Qt.Key_B)
        verify(control1.activeFocus)
        verify(control1.gotit === true)
        verify(control1.editText === "0a")

        keyPress(Qt.Key_B)
        verify(control1.activeFocus)
        verify(control1.gotit === true)
        verify(control1.editText === "0ab")

        test.destroy()
    }

    function test_modelDataChange() {
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox {}', testCase, '');
        comboBox.textRole = "text"
        comboBox.model = model
        compare(comboBox.currentIndex, 0)
        compare(comboBox.currentText, "Banana")
        model.set(0, { text: "Pomegranate", color: "Yellow" })
        compare(comboBox.currentText, "Pomegranate")
        comboBox.destroy()
    }

    function test_qtBug44532() {
        if (Qt.platform.os === "osx")
            skip("When the menu pops up on OS X, it does not return and the test fails after time out")
        var comboBox = Qt.createQmlObject('import QtQuick.Controls 1.2 ; ComboBox { model: ["A", "BB", "CCCCC"] }', container, '')
        var popup = comboBox.__popup
        verify(popup)
        tryCompare(popup, "__popupVisible", false)
        mouseClick(comboBox)
        tryCompare(popup, "__popupVisible", true)
        mouseClick(comboBox)
        tryCompare(popup, "__popupVisible", false)
        comboBox.destroy()
    }
}
}
