/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtTest 1.0
import QtQuick 2.0

Rectangle {
    id: container
    width: 400
    height: 400
    color: "blue"

    Component {
        id: textInputComp
        TextEdit {
            anchors.fill: parent
            visible: true
            focus: true
            color: "white"
        }
    }

    TestCase {
        id: testcase
        name: "tst_plugin"
        when: windowShown

        property var inputPanel: null
        property var handwritingInputPanel: null
        property var textInput: null

        function initTestCase() {
            var inputPanelComp = Qt.createComponent("inputpanel/inputpanel.qml")
            compare(inputPanelComp.status, Component.Ready, "Failed to create component: "+inputPanelComp.errorString())
            inputPanel = inputPanelComp.createObject(container, {"testcase": testcase})
            inputPanel.keyboardLayoutsAvailableSpy.wait()

            var handwritingInputPanelComp = Qt.createComponent("inputpanel/handwritinginputpanel.qml")
            compare(handwritingInputPanelComp.status, Component.Ready, "Failed to create component: "+handwritingInputPanelComp.errorString())
            handwritingInputPanel = handwritingInputPanelComp.createObject(container, {"testcase": testcase, "inputPanel": inputPanel})

            textInput = textInputComp.createObject(container)
        }

        function cleanupTestCase() {
            if (textInput)
                textInput.destroy()
            if (inputPanel)
                inputPanel.destroy()
            if (handwritingInputPanel)
                handwritingInputPanel.destroy()
        }

        function prepareTest(data) {
            container.forceActiveFocus()
            if (data !== undefined && data.hasOwnProperty("initText")) {
                textInput.text = data.initText
                textInput.cursorPosition = data.hasOwnProperty("initCursorPosition") ? data.initCursorPosition : textInput.text.length
                if (data.hasOwnProperty("selectionStart") && data.hasOwnProperty("selectionEnd")) {
                    textInput.select(data.selectionStart, data.selectionEnd)
                }
            } else {
                textInput.text = ""
            }
            textInput.inputMethodHints = data !== undefined && data.hasOwnProperty("initInputMethodHints") ? data.initInputMethodHints : Qt.ImhNone
            handwritingInputPanel.available = false
            textInput.forceActiveFocus()
            inputPanel.setHandwritingMode(false)
            var activeLocales = data !== undefined && data.hasOwnProperty("activeLocales") ? data.activeLocales : []
            inputPanel.setActiveLocales(activeLocales)
            var locale = data !== undefined && data.hasOwnProperty("initLocale") ? data.initLocale : "en_GB"
            if (!inputPanel.isLocaleSupported(locale))
                expectFail("", "Input locale not available (%1)".arg(locale))
            var localeChanged = Qt.inputMethod.locale.name !== locale
            verify(inputPanel.setLocale(locale))
            if (localeChanged && !(textInput.inputMethodHints & Qt.ImhNoPredictiveText))
                wait(300)
            if (data !== undefined && data.hasOwnProperty("initInputMode")) {
                var inputMode = inputPanel.mapInputMode(data.initInputMode)
                if (!inputPanel.isInputModeSupported(inputMode))
                    expectFail("", "Input mode not available (%1)".arg(data.initInputMode))
                verify(inputPanel.setInputMode(inputMode))
            }
            Qt.inputMethod.show()
            waitForRendering(inputPanel)
            verify(inputPanel.visible === true)
        }

        function test_versionCheck_data() {
            return [
                // Note: Add new import versions here
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 1.0; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 1.1; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 1.2; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 1.3; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 2.0; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard 2.1; \
                        Item {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 1.0; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 1.1; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 1.2; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 1.3; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 2.0; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Styles 2.1; \
                        KeyboardStyle {}" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Settings 1.0; \
                        Item { property var styleName: VirtualKeyboardSettings.styleName }" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Settings 1.1; \
                        Item { property var styleName: VirtualKeyboardSettings.styleName }" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Settings 1.2; \
                        Item { property var styleName: VirtualKeyboardSettings.styleName }" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Settings 2.0; \
                        Item { property var styleName: VirtualKeyboardSettings.styleName; \
                               property var locale: VirtualKeyboardSettings.locale; \
                               property var availableLocales: VirtualKeyboardSettings.availableLocales; \
                               property var activeLocales: VirtualKeyboardSettings.activeLocales }" },
                { qml: "import QtQuick 2.0; \
                        import QtQuick.VirtualKeyboard.Settings 2.1; \
                        Item { property var styleName: VirtualKeyboardSettings.styleName; \
                               property var locale: VirtualKeyboardSettings.locale; \
                               property var availableLocales: VirtualKeyboardSettings.availableLocales; \
                               property var activeLocales: VirtualKeyboardSettings.activeLocales }" },
            ]
        }

        function test_versionCheck(data) {
            var obj = null
            var errorMsg
            try {
                obj = Qt.createQmlObject(data.qml, testcase)
            } catch (e) {
                errorMsg = e
            }
            verify(obj !== null, errorMsg)
            if (obj)
                obj.destroy()
        }

        function test_focusShowKeyboard() {
            container.forceActiveFocus()
            verify(inputPanel.visible === false)

            mousePress(textInput, 0, 0)
            waitForRendering(inputPanel)
            mouseRelease(textInput, 0, 0)
            verify(inputPanel.visible === true)

            container.forceActiveFocus()
            verify(inputPanel.visible === false)
        }

        function test_inputMethodShowKeyboard() {
            container.forceActiveFocus()
            Qt.inputMethod.hide()
            verify(inputPanel.visible === false)

            Qt.inputMethod.show()
            waitForRendering(inputPanel)
            verify(inputPanel.visible === true)

            Qt.inputMethod.hide()
            verify(inputPanel.visible === false)
        }

        function test_keyPress_data() {
            return [
                // normal key press
                { initText: "", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: Qt.Key_A, outputKeyCountMin: 1, outputKey: Qt.Key_A, preview: true, outputKeyText: "a", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: false, outputText: "a" },
                { initText: "", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: Qt.Key_AsciiTilde, outputKeyCountMin: 1, outputKey: Qt.Key_AsciiTilde, preview: true, outputKeyText: "~", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: false, outputText: "~" },
                // alternative key press, i.e. long key press
                { initText: "", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: "\u00E4", outputKeyCountMin: 1, outputKey: Qt.Key_A, preview: true, outputKeyText: "\u00E4", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: false, outputText: "\u00E4" },
                // function key press
                { initText: "x", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: Qt.Key_Shift, outputKeyCountMin: 1, outputKey: Qt.Key_Shift, preview: false, outputKeyText: "", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: false, outputText: "x" },
                { initText: "x", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: Qt.Key_Backspace, outputKeyCountMin: 1, outputKey: Qt.Key_Backspace, preview: false, outputKeyText: "", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: false, outputText: "" },
                { initText: "xxxxxx", initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputKey: Qt.Key_Backspace, keyHold: 1000, outputKeyCountMin: 6, outputKey: Qt.Key_Backspace, preview: false, outputKeyText: "", outputKeyModifiers: Qt.NoModifier, outputKeyRepeat: true, outputText: "" },
            ]
        }

        function test_keyPress(data) {
            prepareTest(data)

            inputPanel.characterPreviewBubbleSpy.clear()
            inputPanel.virtualKeyClickedSpy.clear()
            if (data.hasOwnProperty("keyHold")) {
                inputPanel.virtualKeyPress(data.inputKey)
                do {
                    inputPanel.virtualKeyClickedSpy.wait(data.keyHold)
                } while (inputPanel.virtualKeyClickedSpy.count < data.outputKeyCountMin)
                inputPanel.virtualKeyRelease()
            } else {
                inputPanel.virtualKeyClick(data.inputKey)
            }

            compare(inputPanel.characterPreviewBubbleSpy.count, data.preview ? 2 : 0) // show + hide == 2
            verify(inputPanel.virtualKeyClickedSpy.count >= data.outputKeyCountMin)
            for (var i = 0; i < inputPanel.virtualKeyClickedSpy.count; i++) {
                compare(inputPanel.virtualKeyClickedSpy.signalArguments[i][0], data.outputKey)
                compare(inputPanel.virtualKeyClickedSpy.signalArguments[i][1], data.outputKeyText)
                compare(inputPanel.virtualKeyClickedSpy.signalArguments[i][2], data.outputKeyModifiers)
                compare(inputPanel.virtualKeyClickedSpy.signalArguments[i][3], data.outputKeyRepeat)
            }

            compare(textInput.text, data.outputText)
        }

        function test_keyReleaseInaccuracy() {
            prepareTest({ initInputMethodHints: Qt.ImhNoPredictiveText })

            inputPanel.virtualKeyClickedSpy.clear()
            verify(inputPanel.virtualKeyPress(Qt.Key_A))
            var keyObj = inputPanel.findVirtualKey(Qt.Key_A)
            inputPanel.virtualKeyPressPoint = inputPanel.mapFromItem(keyObj, keyObj.width * 1.1, keyObj.height / 2)
            mouseMove(inputPanel, inputPanel.virtualKeyPressPoint.x, inputPanel.virtualKeyPressPoint.y)
            verify(inputPanel.virtualKeyRelease())

            compare(inputPanel.virtualKeyClickedSpy.count, 1)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][0], Qt.Key_A)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][1], "A")
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][2], Qt.ShiftModifier)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][3], false)

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, "A")
        }

        function test_hardKeyInput() {
            prepareTest()

            verify(inputPanel.virtualKeyClick("h"))
            verify(inputPanel.virtualKeyClick("a"))
            verify(inputPanel.virtualKeyClick("r"))
            waitForRendering(textInput)
            if (inputPanel.wordCandidateListVisibleHint)
                compare(textInput.text, "")
            else
                compare(textInput.text, "Har")
            keyClick('d')
            waitForRendering(textInput)
            compare(textInput.text, "Hard")
        }

        function test_inputLocale_data() {
            return [
                { initLocale: "ar_AR", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "\u0645\u0631\u062D\u0628\u0627", outputText: "\u0645\u0631\u062D\u0628\u0627" },
                { initLocale: "fa_FA", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "\u0633\u0644\u0627\u0645", outputText: "\u0633\u0644\u0627\u0645" },
                { initLocale: "da_DK", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hej", outputText: "Hej" },
                { initLocale: "de_DE", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hallo", outputText: "Hallo" },
                { initLocale: "en_GB", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hello", outputText: "Hello" },
                { initLocale: "es_ES", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hola", outputText: "Hola" },
                { initLocale: "hi_IN", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "\u0928\u092E\u0938\u094D\u0915\u093E\u0930", outputText: "\u0928\u092E\u0938\u094D\u0915\u093E\u0930" },
                { initLocale: "fi_FI", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hei", outputText: "Hei" },
                { initLocale: "fr_FR", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "bonjour", outputText: "Bonjour" },
                { initLocale: "it_IT", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "ciao", outputText: "Ciao" },
                { initLocale: "ja_JP", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "watashi", outputText: "\u308F\u305F\u3057" },
                { initLocale: "nb_NO", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hallo", outputText: "Hallo" },
                { initLocale: "pl_PL", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "cze\u015B\u0107", outputText: "Cze\u015B\u0107" },
                { initLocale: "pt_PT", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "ol\u00E1", outputText: "Ol\u00E1" },
                { initLocale: "ru_RU", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "\u043F\u0440\u0438\u0432\u0435\u0442", outputText: "\u041F\u0440\u0438\u0432\u0435\u0442" },
                { initLocale: "sv_SE", initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hall\u00E5", outputText: "Hall\u00E5" }
            ]
        }

        function test_inputLocale(data) {
            prepareTest(data)

            compare(Qt.inputMethod.locale.name, Qt.locale(data.initLocale).name)
            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_inputMethodHints_data() {
            var decmialPoint = Qt.locale().decimalPoint
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhUppercaseOnly, inputSequence: "uppercase text? yes.", outputText: "UPPERCASE TEXT? YES." },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhLowercaseOnly, inputSequence: "uppercase text? no.", outputText: "uppercase text? no." },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDigitsOnly, inputSequence: "1234567890" + decmialPoint, outputText: "1234567890" + decmialPoint },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhFormattedNumbersOnly, inputSequence: "1234567890+-,.()", outputText: "1234567890+-,.()" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDialableCharactersOnly, inputSequence: "1234567890+*#", outputText: "1234567890+*#" },
            ]
        }

        function test_inputMethodHints(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_toggleShift_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 0, inputSequence: "aaa bbb", outputText: "Aaa bbb", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 1, inputSequence: "aaa bbb", outputText: "aaa bbb", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 2, inputSequence: "aaa bbb", outputText: "Aaa bbb", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 3, inputSequence: "aaa bbb", outputText: "AAA BBB", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 4, inputSequence: "aaa bbb", outputText: "aaa bbb", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 5, inputSequence: "aaa bbb", outputText: "Aaa bbb", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 6, inputSequence: "aaa bbb", outputText: "AAA BBB", autoCapitalizationEnabled: true, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, toggleShiftCount: 0, inputSequence: "aaa bbb", outputText: "aaa bbb", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, toggleShiftCount: 1, inputSequence: "aaa bbb", outputText: "AAA BBB", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, toggleShiftCount: 2, inputSequence: "aaa bbb", outputText: "aaa bbb", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, toggleShiftCount: 3, inputSequence: "aaa bbb", outputText: "AAA BBB", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, initLocale: "ar_AR", toggleShiftCount: 0, inputSequence: "\u0645\u0631\u062D\u0628\u0627", outputText: "\u0645\u0631\u062D\u0628\u0627", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
                { initInputMethodHints: Qt.ImhNoPredictiveText, initLocale: "hi_IN", toggleShiftCount: 0, inputSequence: "\u0928\u092E\u0938\u094D\u0915\u093E\u0930", outputText: "\u0928\u092E\u0938\u094D\u0915\u093E\u0930", autoCapitalizationEnabled: false, toggleShiftEnabled: true },
            ]
        }

        function test_toggleShift(data) {
            prepareTest(data)

            compare(inputPanel.autoCapitalizationEnabled, data.autoCapitalizationEnabled)
            compare(inputPanel.toggleShiftEnabled, data.toggleShiftEnabled)
            for (var i = 0; i < data.toggleShiftCount; i++) {
                inputPanel.toggleShift()
            }
            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_focusAndShiftState() {
            prepareTest()

            // Initial focus is set to textInput, shift state should not change
            // when focus is set to container
            inputPanel.shiftStateSpy.clear()
            container.forceActiveFocus()
            compare(inputPanel.shiftStateSpy.count, 0, "Unexpected number of shift state changes after focus change (ImhNone -> ImhNone)")

            // Change to lower case
            inputPanel.shiftStateSpy.clear()
            inputPanel.toggleShift()
            compare(inputPanel.shiftStateSpy.count, 1, "Unexpected number of shift state changes after shift key press")

            // Set focus back to textInput, and expect that shift state is changed
            // to auto upper case
            inputPanel.shiftStateSpy.clear()
            textInput.forceActiveFocus()
            compare(inputPanel.shiftStateSpy.count, 1, "Unexpected number of shift state changes after focus change (auto upper case)")
        }

        function test_symbolMode() {
            prepareTest({ initInputMethodHints: Qt.ImhNoPredictiveText })

            verify(inputPanel.keyboard.symbolMode === false)
            inputPanel.virtualKeyClick(Qt.Key_Context1)
            waitForRendering(inputPanel)
            verify(inputPanel.keyboard.symbolMode === true)
            verify(inputPanel.virtualKeyClick(Qt.Key_Plus))
            verify(inputPanel.keyboard.symbolMode === true)
            verify(inputPanel.virtualKeyClick(Qt.Key_Dollar))
            verify(inputPanel.keyboard.symbolMode === true)
            inputPanel.virtualKeyClick(Qt.Key_Context1)
            waitForRendering(inputPanel)
            verify(inputPanel.keyboard.symbolMode === false)

            Qt.inputMethod.commit()
            compare(textInput.text, "+$")
        }

        function test_dragSymbolMode() {
            prepareTest({ initInputMethodHints: Qt.ImhNoPredictiveText })

            verify(inputPanel.keyboard.symbolMode === false)
            inputPanel.dragSymbolModeSpy.clear()
            inputPanel.virtualKeyPress(Qt.Key_Context1)
            inputPanel.dragSymbolModeSpy.wait()
            waitForRendering(inputPanel)
            verify(inputPanel.keyboardInputArea.dragSymbolMode === true)

            inputPanel.characterPreviewBubbleSpy.clear()
            inputPanel.virtualKeyClickedSpy.clear()
            verify(inputPanel.virtualKeyDrag(Qt.Key_Plus))
            compare(inputPanel.characterPreviewBubbleSpy.count, 1)
            compare(inputPanel.virtualKeyClickedSpy.count, 0)
            inputPanel.virtualKeyRelease()
            waitForRendering(inputPanel)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][0], Qt.Key_Plus)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][1], "+")
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][2], Qt.ShiftModifier)
            compare(inputPanel.virtualKeyClickedSpy.signalArguments[0][3], false)
            verify(inputPanel.keyboardInputArea.dragSymbolMode === false)
            verify(inputPanel.keyboard.symbolMode === false)

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, "+")
        }

        function test_themeChange() {
            prepareTest()

            var origStyle = inputPanel.style()
            inputPanel.setStyle("default")
            inputPanel.styleSpy.clear()

            inputPanel.setStyle("retro")
            inputPanel.styleSpy.wait()
            waitForRendering(inputPanel)

            inputPanel.setStyle("default")
            inputPanel.styleSpy.wait()
            waitForRendering(inputPanel)

            compare(inputPanel.styleSpy.count, 2)

            inputPanel.setStyle(origStyle)
            waitForRendering(inputPanel)
        }

        function test_soundEffects() {
            prepareTest({ initInputMethodHints: Qt.ImhNoPredictiveText })

            wait(500)
            inputPanel.soundEffectSpy.clear()

            verify(inputPanel.virtualKeyClick(Qt.Key_A))
            wait(500)
            if (!inputPanel.keyboard.soundEffect.available || !inputPanel.keyboard.soundEffect.enabled)
                expectFail("", "Sound effects not enabled")
            compare(inputPanel.soundEffectSpy.count, 2)
        }

        function test_navigationKeyInputSequence_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputSequence: "\u00E1\u017C", outputText: "\u00E1\u017C" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputSequence: "~123qwe", outputText: "~123qwe" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase, inputSequence: [ Qt.Key_Shift, Qt.Key_V, Qt.Key_K, Qt.Key_B, Qt.Key_Return ], outputText: "VKB\n" },
            ]
        }

        function test_navigationKeyInputSequence(data) {
            prepareTest(data)

            if (!inputPanel.activateNavigationKeyMode())
                expectFail("", "Arrow key navigation not enabled")
            verify(inputPanel.naviationHighlight.visible)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.navigationKeyClick(data.inputSequence[inputIndex]))
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_navigationCursorWrap_data() {
            return [
                { initialKey: Qt.Key_Q, navigationKey: Qt.Key_Up, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_Q, navigationKey: Qt.Key_Down, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_T, navigationKey: Qt.Key_Up, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_T, navigationKey: Qt.Key_Down, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_Backspace, navigationKey: Qt.Key_Up, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_Backspace, navigationKey: Qt.Key_Down, navigationKeyRepeat: 4 },
                { initialKey: Qt.Key_Backspace, navigationKeySequence: [ Qt.Key_Right, Qt.Key_Left ] },
                { initialKey: Qt.Key_Return, navigationKeySequence: [ Qt.Key_Right, Qt.Key_Left ] },
                { initialKey: Qt.Key_Shift, navigationKeySequence: [ Qt.Key_Left, Qt.Key_Right ] },
                { initialKey: Qt.Key_Context1, navigationKeySequence: [ Qt.Key_Left, Qt.Key_Right ] },
                { initialKey: Qt.Key_Q, navigationKeySequence: [ Qt.Key_Left, Qt.Key_Right ] },
            ]
        }

        function test_navigationCursorWrap(data) {
            prepareTest()

            if (!inputPanel.activateNavigationKeyMode())
                expectFail("", "Arrow key navigation not enabled")
            verify(inputPanel.naviationHighlight.visible)

            verify(inputPanel.navigateToKey(data.initialKey))
            var initialKeyObj = inputPanel.keyboardInputArea.initialKey

            if (data.hasOwnProperty("navigationKey")) {
                for (var i = 0; i < data.navigationKeyRepeat; i++) {
                    inputPanel.emulateNavigationKeyClick(data.navigationKey)
                }
            } else if (data.hasOwnProperty("navigationKeySequence")) {
                for (var navigationKeyIndex in data.navigationKeySequence) {
                    inputPanel.emulateNavigationKeyClick(data.navigationKeySequence[navigationKeyIndex])
                }
            }

            verify(inputPanel.keyboardInputArea.initialKey === initialKeyObj)
        }

        function test_navigationCursorAndWordCandidateView() {
            prepareTest()

            if (!inputPanel.activateNavigationKeyMode())
                expectFail("", "Arrow key navigation not enabled")
            verify(inputPanel.naviationHighlight.visible)

            verify(inputPanel.navigationKeyClick("q"))
            verify(inputPanel.navigationKeyClick("q"))
            wait(200)
            var focusKey = inputPanel.keyboardInputArea.initialKey
            verify(focusKey !== null)

            if (inputPanel.wordCandidateView.count <= 1)
                expectFail("", "Prediction/spell correction not enabled")
            verify(inputPanel.wordCandidateView.count > 1)
            verify(inputPanel.keyboardInputArea.initialKey === focusKey)
            verify(inputPanel.wordCandidateView.currentIndex !== -1)

            // Move focus to word candidate list
            inputPanel.emulateNavigationKeyClick(Qt.Key_Up)
            verify(inputPanel.keyboardInputArea.initialKey === null)
            verify(inputPanel.wordCandidateView.currentIndex !== -1)

            // Move focus to the next item in the list
            var previousHighlightIndex = inputPanel.wordCandidateView.currentIndex
            inputPanel.emulateNavigationKeyClick(Qt.Key_Right)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex + 1)

            // Move focus to previously focused key on keyboard and back
            previousHighlightIndex = inputPanel.wordCandidateView.currentIndex
            inputPanel.emulateNavigationKeyClick(Qt.Key_Down)
            verify(inputPanel.keyboardInputArea.initialKey === focusKey)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)
            inputPanel.emulateNavigationKeyClick(Qt.Key_Up)
            verify(inputPanel.keyboardInputArea.initialKey === null)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)

            // Move focus to last item in the list
            for (previousHighlightIndex = inputPanel.wordCandidateView.currentIndex;
                 previousHighlightIndex < inputPanel.wordCandidateView.count - 1;
                 previousHighlightIndex++) {
                inputPanel.emulateNavigationKeyClick(Qt.Key_Right)
                compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex + 1)
            }

            // Move focus to keyboard (from the end of the word candidate list)
            inputPanel.emulateNavigationKeyClick(Qt.Key_Right)
            verify(inputPanel.keyboardInputArea.initialKey === focusKey)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)

            // Move focus back to last item in the list
            inputPanel.emulateNavigationKeyClick(Qt.Key_Left)
            verify(inputPanel.keyboardInputArea.initialKey === null)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)

            // Move focus to first item in the list
            for (previousHighlightIndex = inputPanel.wordCandidateView.currentIndex;
                 previousHighlightIndex > 0;
                 previousHighlightIndex--) {
                inputPanel.emulateNavigationKeyClick(Qt.Key_Left)
                compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex - 1)
            }

            // Move focus to bottom right corner of keyboard
            inputPanel.emulateNavigationKeyClick(Qt.Key_Left)
            verify(inputPanel.keyboardInputArea.initialKey !== null)
            verify(inputPanel.keyboardInputArea.initialKey !== focusKey)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)

            // Move back to word candidate list by doing an opposite move
            inputPanel.emulateNavigationKeyClick(Qt.Key_Right)
            verify(inputPanel.keyboardInputArea.initialKey === null)
            compare(inputPanel.wordCandidateView.currentIndex, previousHighlightIndex)

            // Select item
            inputPanel.emulateNavigationKeyClick(Qt.Key_Return)
            verify(inputPanel.keyboardInputArea.initialKey !== null)
            verify(inputPanel.wordCandidateView.currentIndex === -1)
            verify(inputPanel.wordCandidateView.count === 0)
            verify(textInput.text.length > 0)
        }

        function test_spellCorrectionSuggestions_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "hwllo", unexpectedSuggestion: "Hello", outputText: "Hwllo" },
                { initInputMethodHints: Qt.ImhNone, inputSequence: "hwllo", expectedSuggestion: "Hello", outputText: "Hello" },
                { initText: "Hello", initInputMethodHints: Qt.ImhNone, inputSequence: "qorld", expectedSuggestion: "world", outputText: "Helloworld" },
                { initText: "isn'", initInputMethodHints: Qt.ImhNone, inputSequence: "t", outputText: "isn't" },
                { initInputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase, inputSequence: "www.example.com", expectedSuggestion: "www.example.com", outputText: "www.example.com" },
                { initInputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase, inputSequence: "user.name@example.com", expectedSuggestion: "user.name@example.com", outputText: "user.name@example.com" },
            ]
        }

        function test_spellCorrectionSuggestions(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }
            waitForRendering(inputPanel)

            if (inputPanel.wordCandidateListVisibleHint) {
                if (data.hasOwnProperty("expectedSuggestion")) {
                    verify(inputPanel.selectionListSearchSuggestion(data.expectedSuggestion, 1000), "The expected spell correction suggestion \"%1\" was not found".arg(data.expectedSuggestion))
                    verify(inputPanel.selectionListSelectCurrentItem(), "Word candidate not selected")
                } else if (data.hasOwnProperty("unexpectedSuggestion")) {
                    verify(!inputPanel.selectionListSearchSuggestion(data.unexpectedSuggestion, 1000), "An unexpected spell correction suggestion \"%1\" was found".arg(data.unexpectedSuggestion))
                    inputPanel.selectionListSelectCurrentItem()
                } else {
                    Qt.inputMethod.commit()
                }
            } else if (textInput.text !== data.outputText) {
                expectFail("", "Prediction/spell correction not enabled")
            }

            compare(textInput.text, data.outputText)
        }

        function test_spellCorrectionAutomaticSpaceInsertion_data() {
            return [
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,'w','o','r','l','d'], outputText: "Hello world" },
                { inputSequence: ['h','e','l','l','o','\'','s',Qt.Key_Select,'w','o','r','l','d'], outputText: "Hello's world" },
                { inputSequence: ['h','e','l','l','o','s','\'',Qt.Key_Select,'w','o','r','l','d'], outputText: "Hellos' world" },
                { inputSequence: ['h','e','l','l','o','-','w','o','r','l','d'], outputText: "Hello-world" },
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,'.','w','o','r','l','d'], outputText: "Hello. World" },
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,',','w','o','r','l','d'], outputText: "Hello, world" },
                { inputSequence: ['h','e','l','l','o','.',Qt.Key_Backspace,'w','o','r','l','d'], outputText: "Helloworld" },
                { inputSequence: ['h','e','l','l','o',' ',Qt.Key_Backspace,'w','o','r','l','d'], outputText: "Helloworld" },
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,'w','o','r','l','d',':-)'], outputText: "Hello world :-)" },
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,'w','o','r','l','d',':-)',':-)'], outputText: "Hello world :-) :-)" },
                { inputSequence: ['h','e','l','l','o',Qt.Key_Select,':-)'], outputText: "Hello :-)" },
                { initText: "Hekko world", selectionStart: 2, selectionEnd: 4, inputSequence: ['l','l'], outputText: "Hello world" },
            ]
        }

        function test_spellCorrectionAutomaticSpaceInsertion(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                var key = data.inputSequence[inputIndex]
                if (key === Qt.Key_Select) {
                    inputPanel.selectionListSelectCurrentItem()
                } else {
                    inputPanel.virtualKeyClick(key)
                }
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            if (!inputPanel.wordCandidateListVisibleHint && textInput.text !== data.outputText)
                expectFail("", "Prediction/spell correction not enabled")
            compare(textInput.text, data.outputText)
        }

        function test_selectionListSelectInvalidItem() {
            prepareTest()

            // Note: This test passes if it does not crash
            if (inputPanel.wordCandidateView.model) {
                compare(inputPanel.wordCandidateView.count, 0)
                inputPanel.wordCandidateView.model.selectItem(-2)
                inputPanel.wordCandidateView.model.selectItem(-1)
                inputPanel.wordCandidateView.model.selectItem(0)
                inputPanel.wordCandidateView.model.selectItem(1)
                inputPanel.wordCandidateView.model.selectItem(2)
            }
        }

        function test_pinyinInputMethod_data() {
            return [
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_CN", inputSequence: "suoer", expectedCandidates: [ "\u7D22\u5C14" ], outputText: "\u7D22\u5C14" },
                // Build phase from individual candidates
                // Note: this phrase does not exist in the system dictionary
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_CN", inputSequence: "bailou", expectedCandidates: [ "\u5457", "\u5A04" ], outputText: "\u5457\u5A04" },
                // Select phrase from the user dictinary
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_CN", inputSequence: "bailou", expectedCandidates: [ "\u5457\u5A04" ], outputText: "\u5457\u5A04" },
                // Add an apostrophe before joined syllables in cases of ambiguity, disable the user dictionary (Qt.ImhSensitiveData) so it does not affect to the results
                { initInputMethodHints: Qt.ImhNone | Qt.ImhSensitiveData, initLocale: "zh_CN", inputSequence: "zhangang", expectedCandidates: [ "\u5360", "\u94A2" ], outputText: "\u5360\u94A2" },
                { initInputMethodHints: Qt.ImhNone | Qt.ImhSensitiveData, initLocale: "zh_CN", inputSequence: "zhang'ang", expectedCandidates: [ "\u7AE0", "\u6602" ], outputText: "\u7AE0\u6602" },
            ]
        }

        function test_pinyinInputMethod(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }
            waitForRendering(inputPanel)

            for (var candidateIndex in data.expectedCandidates) {
                verify(inputPanel.selectionListSearchSuggestion(data.expectedCandidates[candidateIndex]))
                verify(inputPanel.selectionListSelectCurrentItem())
            }

            compare(textInput.text, data.outputText)
        }

        function test_cangjieInputMethod_data() {
            return [
                // "vehicle"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\u5341", expectedCandidates: [ "\u8ECA" ], outputText: "\u8ECA" },
                // simplified mode: "vehicle"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: true, inputSequence: "\u5341\u5341", expectedCandidates: [ "\u8ECA" ], outputText: "\u8ECA" },
                // "to thank"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u535C\u53E3\u7AF9\u7AF9\u6208", expectedCandidates: [ "\u8B1D" ], outputText: "\u8B1D" },
                // exceptions: "door"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u65E5\u5F13", expectedCandidates: [ "\u9580" ], outputText: "\u9580" },
                // exceptions: "small table"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u7AF9\u5F13", expectedCandidates: [ "\u51E0" ], outputText: "\u51E0" },
                // fixed decomposition
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u7AF9\u96E3", expectedCandidates: [ "\u81FC" ], outputText: "\u81FC" },
                // input handling: valid input sequence + space
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\u5341 ", outputText: "\u8ECA" },
                // input handling: invalid input sequence + space
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530 ", outputText: "" },
                // input handling: valid input sequence + enter
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\u5341\n", outputText: "\u8ECA\n" },
                // input handling: invalid input sequence + enter
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\n", outputText: "\n" },
                // input handling: valid input sequence + punctuation
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\u5341\uFF0E", outputText: "\u8ECA\uFF0E" },
                // input handling: invalid input sequence + punctuation
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: "\u5341\u7530\uFF0E", outputText: "\uFF0E" },
                // phrase suggestions: select by selection list
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: ['\u5341', '\u7530', '\u5341', Qt.Key_Select], expectedCandidates: [ "\u8F1B" ], outputText: "\u8ECA\u8F1B" },
                // phrase suggestions: select by space
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Cangjie", initSimplified: false, inputSequence: ['\u5341', '\u7530', '\u5341', ' '], expectedCandidates: [ "\u8F1B" ], outputText: "\u8ECA\u8F1B" },
            ]
        }

        function test_cangjieInputMethod(data) {
            prepareTest(data)

            if (data.hasOwnProperty("initSimplified")) {
                if (inputPanel.inputMethod.simplified !== data.initSimplified)
                    verify(inputPanel.virtualKeyClick(Qt.Key_Mode_switch))
                verify(inputPanel.inputMethod.simplified === data.initSimplified)
            }

            for (var inputIndex in data.inputSequence) {
                var key = data.inputSequence[inputIndex]
                if (key === Qt.Key_Select)
                    inputPanel.selectionListSelectCurrentItem()
                else
                    verify(inputPanel.virtualKeyClick(key))
            }
            waitForRendering(inputPanel)

            if (data.expectedCandidates) {
                for (var candidateIndex in data.expectedCandidates) {
                    verify(inputPanel.selectionListSearchSuggestion(data.expectedCandidates[candidateIndex]))
                    verify(inputPanel.selectionListSelectCurrentItem())
                }
            }

            compare(textInput.text, data.outputText)
        }

        function test_zhuyinInputMethod_data() {
            return [
                // "Bottle"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: [ "ㄆㄧㄥˊ", "ㄗ˙" ], expectedCandidates: [ "瓶", "子" ], outputText: "瓶子" },
                // "Hello you"
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: [ "ㄋㄧˇ", "ㄏㄠˇ" ], expectedCandidates: [ "你", "好" ], outputText: "你好" },
                // "Hello you" (select by space)
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄧˇ ㄏㄠˇ ", outputText: "你好" },
                // Incomplete input sequence + space (no suggestions, no output)
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄧ ", outputText: "" },
                // Test all zhuyin symbols
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄅㄚ", expectedCandidates: "八", outputText: "八" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄆㄚˊ", expectedCandidates: "杷", outputText: "杷" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄇㄚˇ", expectedCandidates: "馬", outputText: "馬" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄈㄚˇ", expectedCandidates: "法", outputText: "法" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄉㄧˋ", expectedCandidates: "地", outputText: "地" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄊㄧˊ", expectedCandidates: "提", outputText: "提" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄧˇ", expectedCandidates: "你", outputText: "你" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄌㄧˋ", expectedCandidates: "利", outputText: "利" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄍㄠˋ", expectedCandidates: "告", outputText: "告" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄎㄠˇ", expectedCandidates: "考", outputText: "考" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄏㄠˇ", expectedCandidates: "好", outputText: "好" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄐㄧㄠˋ", expectedCandidates: "叫", outputText: "叫" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄑㄧㄠˇ", expectedCandidates: "巧", outputText: "巧" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄒㄧㄠˇ", expectedCandidates: "小", outputText: "小" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄓㄨˇ", expectedCandidates: "主", outputText: "主" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄔㄨ", expectedCandidates: "出", outputText: "出" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄨˋ", expectedCandidates: "束", outputText: "束" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄖㄨˋ", expectedCandidates: "入", outputText: "入" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄗㄞˋ", expectedCandidates: "在", outputText: "在" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄘㄞˊ", expectedCandidates: "才", outputText: "才" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄙㄞ", expectedCandidates: "塞", outputText: "塞" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄉㄚˋ", expectedCandidates: "大", outputText: "大" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄉㄨㄛ", expectedCandidates: "多", outputText: "多" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄉㄜˊ", expectedCandidates: "得", outputText: "得" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄉㄧㄝ", expectedCandidates: "爹", outputText: "爹" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄞˋ", expectedCandidates: "晒", outputText: "晒" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄟˊ", expectedCandidates: "誰", outputText: "誰" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄠˇ", expectedCandidates: "少", outputText: "少" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄡ", expectedCandidates: "收", outputText: "收" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄢ", expectedCandidates: "山", outputText: "山" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄣ", expectedCandidates: "申", outputText: "申" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄤˋ", expectedCandidates: "上", outputText: "上" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄕㄥ", expectedCandidates: "生", outputText: "生" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄦˊ", expectedCandidates: "而", outputText: "而" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄧˋ", expectedCandidates: "逆", outputText: "逆" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄨˇ", expectedCandidates: "努", outputText: "努" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄋㄩˇ", expectedCandidates: "女", outputText: "女" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄗ", expectedCandidates: "資", outputText: "資" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄓ", expectedCandidates: "知", outputText: "知" },
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: "ㄙˇ", expectedCandidates: "死", outputText: "死" },
                // Symbol mode change during composing state (should not cause interruption to the composing state)
                { initInputMethodHints: Qt.ImhNone, initLocale: "zh_TW", initInputMode: "Zhuyin", inputSequence: [ "ㄋㄧ", Qt.Key_Context1, Qt.Key_Context1, "ˇ" ], expectedCandidates: [ "", "", "", "你" ], outputText: "你" },
            ]
        }

        function test_zhuyinInputMethod(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                if (Array.isArray(data.inputSequence)) {
                    var inputSequence = data.inputSequence[inputIndex]
                    for (var charIndex in inputSequence) {
                        verify(inputPanel.virtualKeyClick(inputSequence[charIndex]))
                    }

                    waitForRendering(inputPanel)

                    if (data.expectedCandidates && inputIndex < data.expectedCandidates.length && data.expectedCandidates[inputIndex].length > 0) {
                        verify(inputPanel.selectionListSearchSuggestion(data.expectedCandidates[inputIndex]))
                        verify(inputPanel.selectionListSelectCurrentItem())
                    }
                } else {
                    verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
                }
            }
            waitForRendering(inputPanel)

            if (!Array.isArray(data.inputSequence) && data.expectedCandidates) {
                verify(inputPanel.selectionListSearchSuggestion(data.expectedCandidates))
                verify(inputPanel.selectionListSelectCurrentItem())
            }

            Qt.inputMethod.commit()
            compare(textInput.text, data.outputText)
        }

        function test_hangulInputMethod_data() {
            return [
                // Test boundaries of the Hangul Jamo BMP plane
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F", outputText: "\uAC00" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3131", outputText: "\uAC01" },
                { initLocale: "ko_KR", inputSequence: "\u314E\u3163", outputText: "\uD788" },
                { initLocale: "ko_KR", inputSequence: "\u314E\u3163\u314E", outputText: "\uD7A3" },
                // Test double medial Jamos
                { initLocale: "ko_KR", inputSequence: "\u3131\u3157\u314F", outputText: "\uACFC" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u3157\u3150", outputText: "\uAD18" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u3157\u3163", outputText: "\uAD34" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u315C\u3153", outputText: "\uAD88" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u315C\u3154", outputText: "\uADA4" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u315C\u3163", outputText: "\uADC0" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u3161\u3163", outputText: "\uAE14" },
                // Test double final Jamos
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3131\u3145", outputText: "\uAC03" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3134\u3148", outputText: "\uAC05" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3134\u314E", outputText: "\uAC06" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u3131", outputText: "\uAC09" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u3141", outputText: "\uAC0A" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u3142", outputText: "\uAC0B" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u3145", outputText: "\uAC0C" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u314C", outputText: "\uAC0D" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u314D", outputText: "\uAC0E" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3139\u314E", outputText: "\uAC0F" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3142\u3145", outputText: "\uAC12" },
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3145\u3145", outputText: "\uAC14" },
                // Test using the final Jamo of the first syllable as an initial
                // Jamo of the following syllable
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3131\u314F", outputText: "\uAC00\uAC00" },
                // Test splitting the final double Jamo and use the second Jamo of
                // the split Jamo as initial Jamo of the following syllable
                { initLocale: "ko_KR", inputSequence: "\u3131\u314F\u3131\u3145\u314F", outputText: "\uAC01\uC0AC" },
                // Test entering a Jamo syllable with surrounding text
                { initLocale: "ko_KR", initText: "abcdef", initCursorPosition: 3, inputSequence: "\u3131\u314F\u3131", outputText: "abc\uAC01def" },
            ]
        }

        function test_hangulInputMethod(data) {
            prepareTest(data)

            compare(Qt.inputMethod.locale.name, Qt.locale(data.initLocale).name)

            // Add Jamos one by one
            var intermediateResult = []
            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
                intermediateResult.push(textInput.text)
            }

            compare(textInput.text, data.outputText)

            // Remove Jamos one by one.
            // The number of removed characters must match to the number of Jamos entered.
            for (; inputIndex >= 0; inputIndex--) {
                compare(textInput.text, intermediateResult.pop())
                inputPanel.virtualKeyClick(Qt.Key_Backspace)
            }

            waitForRendering(inputPanel)
            compare(textInput.text, data.initText !== undefined ? data.initText : "")
        }

        function test_japaneseInputModes_data() {
            return [
                // Hiragana
                { initLocale: "ja_JP", initInputMode: "Hiragana", inputSequence: ["n","i","h","o","n","g","o",Qt.Key_Return], outputText: "\u306B\u307B\u3093\u3054" },
                // Hiragana to Kanjie conversion
                { initLocale: "ja_JP", initInputMode: "Hiragana", inputSequence: ["n","i","h","o","n","g","o",Qt.Key_Space,Qt.Key_Return], outputText: "\u65E5\u672C\u8A9E" },
                // Correction to Hiragana sequence using exact match mode
                { initLocale: "ja_JP", initInputMode: "Hiragana", inputSequence: [
                                // Write part of the text leaving out "ni" from the beginning
                                "h","o","n","g","o",
                                // Activate the exact mode and move the cursor to beginning
                                Qt.Key_Left,Qt.Key_Left,Qt.Key_Left,Qt.Key_Left,
                                // Enter the missing text
                                "n","i",
                                // Move the cursor to end
                                Qt.Key_Right,Qt.Key_Right,Qt.Key_Right,Qt.Key_Right,Qt.Key_Right,
                                // Do Kanjie conversion
                                Qt.Key_Space,
                                // Choose the first candidate
                                Qt.Key_Return], outputText: "\u65E5\u672C\u8A9E" },
                // Katakana
                { initLocale: "ja_JP", initInputMode: "Katakana", inputSequence: ["a","m","e","r","i","k","a"], outputText: "\u30A2\u30E1\u30EA\u30AB" },
                // Toggle symbol mode without affecting the input method state
                { initLocale: "ja_JP", initInputMode: "Hiragana", inputSequence: ["n","i","h","o","n","g","o"], outputText: "" },
                // Latin only
                { initLocale: "ja_JP", initInputMethodHints: Qt.ImhLatinOnly, inputSequence: "hello", outputText: "Hello" },
            ]
        }

        function test_japaneseInputModes(data) {
            prepareTest(data)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.virtualKeyClick(data.inputSequence[inputIndex]))
            }

            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_baseKeyNoModifier() {
            // The Japanese keyboard uses the BaseKey.noModifier flag for the arrow keys.
            // Without this flag the arrow key + shift would extend the text selection.
            prepareTest({ initLocale: "ja_JP", initInputMethodHints: Qt.ImhLatinOnly })
            verify(inputPanel.virtualKeyClick("a"))
            verify(inputPanel.virtualKeyClick(Qt.Key_Left))
            compare(textInput.cursorPosition, 0)
            verify(inputPanel.virtualKeyClick(Qt.Key_Right))
            compare(textInput.cursorPosition, 1)
            compare(textInput.selectedText, "")
        }

        function test_hwrInputSequence_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 0, inputSequence: "abcdefghij", outputText: "Abcdefghij" },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 1, inputSequence: "klmnopqrst", outputText: "klmnopqrst" },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 3, inputSequence: "uvwxyz", outputText: "UVWXYZ" },
            ]
        }

        function test_hwrInputSequence(data) {
            prepareTest(data)

            if (!inputPanel.setHandwritingMode(true))
                expectFail("", "Handwriting not enabled")
            verify(inputPanel.handwritingMode === true)

            for (var i = 0; i < data.toggleShiftCount; i++) {
                inputPanel.toggleShift()
            }
            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.emulateHandwriting(data.inputSequence.charAt(inputIndex), true))
            }

            if (inputPanel.wordCandidateView.count > 0) {
                if (inputPanel.selectionListSearchSuggestion(data.outputText)) {
                    inputPanel.selectionListSelectCurrentItem()
                }
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)
        }

        function test_hwrNumericInputSequence_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferNumbers, modeSwitchAllowed: true, inputSequence: "0123456789", outputText: "0123456789" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDigitsOnly, modeSwitchAllowed: false, inputSequence: "1234567890", outputText: "1234567890" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhFormattedNumbersOnly, modeSwitchAllowed: false, inputSequence: "1234567890+", outputText: "1234567890+" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDialableCharactersOnly, modeSwitchAllowed: false, inputSequence: "1234567890+", outputText: "1234567890+" },
            ]
        }

        function test_hwrNumericInputSequence(data) {
            prepareTest(data)

            if (!inputPanel.setHandwritingMode(true))
                expectFail("", "Handwriting not enabled")
            verify(inputPanel.handwritingMode === true)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.emulateHandwriting(data.inputSequence.charAt(inputIndex), true))
            }

            if (inputPanel.wordCandidateView.count > 0) {
                if (inputPanel.selectionListSearchSuggestion(data.outputText)) {
                    inputPanel.selectionListSelectCurrentItem()
                }
            }

            Qt.inputMethod.commit()
            waitForRendering(inputPanel)
            compare(textInput.text, data.outputText)

            var inputMode = inputPanel.inputMode
            verify(inputPanel.virtualKeyClick(Qt.Key_Mode_switch))
            waitForRendering(inputPanel)
            compare(inputPanel.inputMode !== inputMode, data.modeSwitchAllowed)
        }

        function test_hwrSpellCorrectionSuggestions_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: "bello", unexpectedSuggestion: "Hello", outputText: "Bello" },
                { initInputMethodHints: Qt.ImhNone, inputSequence: "bello", expectedSuggestion: "Hello", outputText: "Hello" },
                { initText: "Hello", initInputMethodHints: Qt.ImhNone, inputSequence: "worla", expectedSuggestion: "world", outputText: "Helloworld" },
                { initText: "isn'", initInputMethodHints: Qt.ImhNone, inputSequence: "t", outputText: "isn't" },
                { initInputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase, inputSequence: "www.example.com", expectedSuggestion: "www.example.com", outputText: "www.example.com" },
                { initInputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase, inputSequence: "user.name@example.com", expectedSuggestion: "user.name@example.com", outputText: "user.name@example.com" },
            ]
        }

        function test_hwrSpellCorrectionSuggestions(data) {
            prepareTest(data)

            if (!inputPanel.setHandwritingMode(true))
                expectFail("", "Handwriting not enabled")
            verify(inputPanel.handwritingMode === true)

            for (var inputIndex in data.inputSequence) {
                verify(inputPanel.emulateHandwriting(data.inputSequence.charAt(inputIndex), true))
            }
            waitForRendering(inputPanel)

            if (inputPanel.wordCandidateListVisibleHint) {
                if (data.hasOwnProperty("expectedSuggestion")) {
                    verify(inputPanel.selectionListSearchSuggestion(data.expectedSuggestion, 1000), "The expected spell correction suggestion \"%1\" was not found".arg(data.expectedSuggestion))
                    verify(inputPanel.selectionListSelectCurrentItem(), "Word candidate not selected")
                } else if (data.hasOwnProperty("unexpectedSuggestion")) {
                    verify(!inputPanel.selectionListSearchSuggestion(data.unexpectedSuggestion, 1000), "An unexpected spell correction suggestion \"%1\" was found".arg(data.unexpectedSuggestion))
                    inputPanel.selectionListSelectCurrentItem()
                } else {
                    Qt.inputMethod.commit()
                }
            } else if (textInput.text !== data.outputText) {
                expectFail("", "Prediction/spell correction not enabled")
            }

            compare(textInput.text, data.outputText)
        }

        function test_hwrFullScreenInputSequence_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 0, inputSequence: "abcdefghij", outputText: "Abcdefghij" },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 1, inputSequence: "klmnopqrst", outputText: "klmnopqrst" },
                { initInputMethodHints: Qt.ImhNoPredictiveText, toggleShiftCount: 3, inputSequence: "uvwxyz", outputText: "UVWXYZ" },
            ]
        }

        function test_hwrFullScreenInputSequence(data) {
            prepareTest(data)

            if (!handwritingInputPanel.enabled)
                expectFail("", "Handwriting not enabled")
            verify(handwritingInputPanel.enabled)
            handwritingInputPanel.available = true
            verify(inputPanel.visible === false)

            for (var i = 0; i < data.toggleShiftCount; i++) {
                inputPanel.toggleShift()
            }
            for (var inputIndex in data.inputSequence) {
                verify(handwritingInputPanel.emulateHandwriting(data.inputSequence[inputIndex], true))
            }

            if (handwritingInputPanel.wordCandidatePopupListSearchSuggestion(data.outputText)) {
                handwritingInputPanel.wordCandidatePopupListSelectCurrentItem()
            }

            Qt.inputMethod.commit()
            compare(textInput.text, data.outputText)
        }

        function test_hwrFullScreenNumericInputSequence_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferNumbers, inputSequence: "0123456789", outputText: "0123456789" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDigitsOnly, inputSequence: "1234567890", outputText: "1234567890" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhFormattedNumbersOnly, inputSequence: "1234567890+", outputText: "1234567890+" },
                { initInputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhDialableCharactersOnly, inputSequence: "1234567890+", outputText: "1234567890+" },
            ]
        }

        function test_hwrFullScreenNumericInputSequence(data) {
            prepareTest(data)

            if (!handwritingInputPanel.enabled)
                expectFail("", "Handwriting not enabled")
            verify(handwritingInputPanel.enabled)
            handwritingInputPanel.available = true
            verify(inputPanel.visible === false)

            for (var inputIndex in data.inputSequence) {
                verify(handwritingInputPanel.emulateHandwriting(data.inputSequence.charAt(inputIndex), true))
            }

            if (handwritingInputPanel.wordCandidatePopupListSearchSuggestion(data.outputText)) {
                handwritingInputPanel.wordCandidatePopupListSelectCurrentItem()
            }

            Qt.inputMethod.commit()
            compare(textInput.text, data.outputText)
        }

        function test_hwrFullScreenGestures_data() {
            return [
                { initInputMethodHints: Qt.ImhNoPredictiveText, inputSequence: ["a","b","c",Qt.Key_Backspace,Qt.Key_Space,"c"], outputText: "Ab c" },
            ]
        }

        function test_hwrFullScreenGestures(data) {
            prepareTest(data)

            if (!handwritingInputPanel.enabled)
                expectFail("", "Handwriting not enabled")
            verify(handwritingInputPanel.enabled)
            handwritingInputPanel.available = true
            verify(inputPanel.visible === false)

            for (var inputIndex in data.inputSequence) {
                verify(handwritingInputPanel.emulateHandwriting(data.inputSequence[inputIndex], true))
            }

            Qt.inputMethod.commit()
            compare(textInput.text, data.outputText)
        }

        function test_hwrFullScreenWordCandidatePopup_data() {
            return [
                { inputSequence: "hello", initLinesToBottom: 0, popupFlipped: true },
                { inputSequence: "hello", initLinesToBottom: 2, popupFlipped: true },
                { inputSequence: "hello", initLinesToBottom: 4, popupFlipped: false },
                { inputSequence: "hello", initLinesToBottom: 5, popupFlipped: false },
            ]
        }

        function test_hwrFullScreenWordCandidatePopup(data) {
            prepareTest(data)

            if (!handwritingInputPanel.enabled)
                skip("Handwriting not enabled")
            handwritingInputPanel.available = true
            if (!inputPanel.wordCandidateListVisibleHint)
                skip("Word candidates not available (spell correction/hwr suggestions)")

            var numAddedLines = Math.floor(textInput.height / Qt.inputMethod.cursorRectangle.height - data.initLinesToBottom) - 1
            for (var i = 0; i < numAddedLines; i++) {
                textInput.insert(textInput.length, "\n")
            }
            compare(textInput.lineCount - 1, numAddedLines)

            for (var inputIndex in data.inputSequence) {
                verify(handwritingInputPanel.emulateHandwriting(data.inputSequence.charAt(inputIndex), true))
            }
            waitForRendering(handwritingInputPanel)

            if (data.popupFlipped) {
                verify(handwritingInputPanel.wordCandidatePopupList.y + handwritingInputPanel.wordCandidatePopupList.height <= Qt.inputMethod.cursorRectangle.y)
            } else {
                verify(handwritingInputPanel.wordCandidatePopupList.y >= Qt.inputMethod.cursorRectangle.y + Qt.inputMethod.cursorRectangle.height)
                verify(handwritingInputPanel.wordCandidatePopupList.y + handwritingInputPanel.wordCandidatePopupList.height < textInput.height)
            }
        }

        function test_availableLocales() {
            verify(inputPanel.availableLocales.length > 0)
        }

        function test_activeLocales_data() {
            return [
                { activeLocales: ["invalid"], expectedLocale: "en_GB" },
                { activeLocales: ["fi_FI"], initLocale: "fi_FI" },
                { activeLocales: ["en_GB"], initLocale: "en_GB" },
                { activeLocales: ["en_GB", "fi_FI", "ar_AR"] },
            ]
        }

        function test_activeLocales(data) {
            prepareTest(data)
            for (var i = 0; i < inputPanel.availableLocales.length; i++) {
                var locale = inputPanel.availableLocales[i]
                var expectedResult
                if (data.hasOwnProperty("expectedLocale"))
                    expectedResult = locale === data.expectedLocale
                else
                    expectedResult = (inputPanel.activeLocales.length === 0 || inputPanel.activeLocales.indexOf(locale) !== -1) && inputPanel.availableLocales.indexOf(locale) !== -1
                inputPanel.setLocale(locale)
                waitForRendering(inputPanel)
                compare(inputPanel.locale === locale, expectedResult, "Test locale %1".arg(locale))
            }
        }

        function test_wordReselection_data() {
            return [
                { initText: "hello", clickPositions: [5], expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hello" },
                { initText: "hello", clickPositions: [4], expectedPreeditText: "hello", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hello", clickPositions: [1], expectedPreeditText: "hello", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hello", clickPositions: [0], expectedPreeditText: "", expectedCursorPosition: 0, expectedText: "hello" },
                { initText: "hello", clickPositions: [4, 3], expectedPreeditText: "hel", expectedCursorPosition: 0, expectedText: "lo" },
                // 5
                { initText: "hello", clickPositions: [4, 2], expectedPreeditText: "he", expectedCursorPosition: 0, expectedText: "llo" },
                { initText: "hello", clickPositions: [4, 1], expectedPreeditText: "h", expectedCursorPosition: 0, expectedText: "ello" },
                { initText: "hello", clickPositions: [4, 0], expectedPreeditText: "", expectedCursorPosition: 0, expectedText: "hello" },
                { initText: "hello", clickPositions: [1, 2], expectedPreeditText: "he", expectedCursorPosition: 0, expectedText: "llo" },
                { initText: "hello", clickPositions: [1, 2, 2], expectedPreeditText: "", expectedCursorPosition: 2, expectedText: "hello" },
                // 10
                { initText: "hello", clickPositions: [1, 5], expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hello" },
                { initText: "hel-lo", clickPositions: [3], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hel-lo", clickPositions: [4], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hel-lo", clickPositions: [4, 4], expectedPreeditText: "", expectedCursorPosition: 4, expectedText: "hel-lo" },
                { initText: "hel-lo", clickPositions: [5], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                // 15
                { initText: "hel-lo", clickPositions: [5], initInputMethodHints: Qt.ImhNoPredictiveText, expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hel-lo" },
                { initText: "isn'", clickPositions: [2], expectedPreeditText: "isn", expectedCursorPosition: 0, expectedText: "'" },
                { initText: "isn't", clickPositions: [2], expectedPreeditText: "isn't", expectedCursorPosition: 0, expectedText: "" },
                { initText: "-hello", clickPositions: [2], expectedPreeditText: "hello", expectedCursorPosition: 1, expectedText: "-" },
                { initText: "aa http://www.example.com bb", clickPositions: [4], expectedPreeditText: "http", expectedCursorPosition: 3, expectedText: "aa ://www.example.com bb" },
                // 20
                { initText: "aa http://www.example.com bb", initInputMethodHints: Qt.ImhUrlCharactersOnly, clickPositions: [4], expectedPreeditText: "http://www.example.com", expectedCursorPosition: 3, expectedText: "aa  bb" },
                { initText: "aa username@example.com bb", clickPositions: [4], expectedPreeditText: "username", expectedCursorPosition: 3, expectedText: "aa @example.com bb" },
                { initText: "aa username@example.com bb", initInputMethodHints: Qt.ImhEmailCharactersOnly, clickPositions: [4], expectedPreeditText: "username@example.com", expectedCursorPosition: 3, expectedText: "aa  bb" },
            ]
        }

        function test_wordReselection(data) {
            prepareTest(data)

            var cursorRects = []
            for (var i = 0; i < data.clickPositions.length; i++)
                cursorRects.push(textInput.positionToRectangle(data.clickPositions[i]))

            for (i = 0; i < data.clickPositions.length; i++) {
                var cursorRect = cursorRects[i]
                mousePress(textInput, cursorRect.x, cursorRect.y + cursorRect.height / 2, Qt.LeftButton, Qt.NoModifier, 20)
                mouseRelease(textInput, cursorRect.x, cursorRect.y + cursorRect.height / 2, Qt.LeftButton, Qt.NoModifier, 20)
                waitForRendering(textInput)
            }

            if (!inputPanel.wordCandidateListVisibleHint && inputPanel.preeditText !== data.expectedPreeditText)
                expectFail("", "Prediction/spell correction not enabled")
            compare(inputPanel.preeditText, data.expectedPreeditText)

            if (!inputPanel.wordCandidateListVisibleHint && inputPanel.cursorPosition !== data.expectedCursorPosition)
                expectFail("", "Prediction/spell correction not enabled")
            compare(inputPanel.cursorPosition, data.expectedCursorPosition)

            if (!inputPanel.wordCandidateListVisibleHint && textInput.text !== data.expectedText)
                expectFail("", "Prediction/spell correction not enabled")
            compare(textInput.text, data.expectedText)
        }

        function test_hwrWordReselection_data() {
            return [
                { initText: "hello", clickPositions: [5], expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hello" },
                { initText: "hello", clickPositions: [4], expectedPreeditText: "hello", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hello", clickPositions: [1], expectedPreeditText: "hello", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hello", clickPositions: [0], expectedPreeditText: "", expectedCursorPosition: 0, expectedText: "hello" },
                { initText: "hello", clickPositions: [4, 3], expectedPreeditText: "hel", expectedCursorPosition: 0, expectedText: "lo" },
                // 5
                { initText: "hello", clickPositions: [4, 2], expectedPreeditText: "he", expectedCursorPosition: 0, expectedText: "llo" },
                { initText: "hello", clickPositions: [4, 1], expectedPreeditText: "h", expectedCursorPosition: 0, expectedText: "ello" },
                { initText: "hello", clickPositions: [4, 0], expectedPreeditText: "", expectedCursorPosition: 0, expectedText: "hello" },
                { initText: "hello", clickPositions: [1, 2], expectedPreeditText: "he", expectedCursorPosition: 0, expectedText: "llo" },
                { initText: "hello", clickPositions: [1, 2, 2], expectedPreeditText: "", expectedCursorPosition: 2, expectedText: "hello" },
                // 10
                { initText: "hello", clickPositions: [1, 5], expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hello" },
                { initText: "hel-lo", clickPositions: [3], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hel-lo", clickPositions: [4], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                { initText: "hel-lo", clickPositions: [4, 4], expectedPreeditText: "", expectedCursorPosition: 4, expectedText: "hel-lo" },
                { initText: "hel-lo", clickPositions: [5], expectedPreeditText: "hel-lo", expectedCursorPosition: 0, expectedText: "" },
                // 15
                { initText: "hel-lo", clickPositions: [5], initInputMethodHints: Qt.ImhNoPredictiveText, expectedPreeditText: "", expectedCursorPosition: 5, expectedText: "hel-lo" },
                { initText: "isn'", clickPositions: [2], expectedPreeditText: "isn", expectedCursorPosition: 0, expectedText: "'" },
                { initText: "isn't", clickPositions: [2], expectedPreeditText: "isn't", expectedCursorPosition: 0, expectedText: "" },
                { initText: "-hello", clickPositions: [2], expectedPreeditText: "hello", expectedCursorPosition: 1, expectedText: "-" },
                { initText: "aa http://www.example.com bb", clickPositions: [4], expectedPreeditText: "http", expectedCursorPosition: 3, expectedText: "aa ://www.example.com bb" },
                // 20
                { initText: "aa http://www.example.com bb", initInputMethodHints: Qt.ImhUrlCharactersOnly, clickPositions: [4], expectedPreeditText: "http://www.example.com", expectedCursorPosition: 3, expectedText: "aa  bb" },
                { initText: "aa username@example.com bb", clickPositions: [4], expectedPreeditText: "username", expectedCursorPosition: 3, expectedText: "aa @example.com bb" },
                { initText: "aa username@example.com bb", initInputMethodHints: Qt.ImhEmailCharactersOnly, clickPositions: [4], expectedPreeditText: "username@example.com", expectedCursorPosition: 3, expectedText: "aa  bb" },
            ]
        }

        function test_hwrWordReselection(data) {
            prepareTest(data)

            if (!inputPanel.setHandwritingMode(true))
                skip("Handwriting not enabled")

            var cursorRects = []
            for (var i = 0; i < data.clickPositions.length; i++)
                cursorRects.push(textInput.positionToRectangle(data.clickPositions[i]))

            for (i = 0; i < data.clickPositions.length; i++) {
                var cursorRect = cursorRects[i]
                mousePress(textInput, cursorRect.x, cursorRect.y + cursorRect.height / 2, Qt.LeftButton, Qt.NoModifier, 20)
                mouseRelease(textInput, cursorRect.x, cursorRect.y + cursorRect.height / 2, Qt.LeftButton, Qt.NoModifier, 20)
                waitForRendering(textInput)
            }

            if (!inputPanel.wordCandidateListVisibleHint && inputPanel.preeditText !== data.expectedPreeditText)
                expectFail("", "Prediction/spell correction not enabled")
            compare(inputPanel.preeditText, data.expectedPreeditText)

            if (!inputPanel.wordCandidateListVisibleHint && inputPanel.cursorPosition !== data.expectedCursorPosition)
                expectFail("", "Prediction/spell correction not enabled")
            compare(inputPanel.cursorPosition, data.expectedCursorPosition)

            if (!inputPanel.wordCandidateListVisibleHint && textInput.text !== data.expectedText)
                expectFail("", "Prediction/spell correction not enabled")
            compare(textInput.text, data.expectedText)
        }

        function test_selection_data() {
            return [
                { initText: "Hello cruel world", selectionStart: 2, selectionEnd: 9, expectHandlesToBeVisible: true },
                { initText: "Hello cruel world", selectionStart: 9, selectionEnd: 9, expectHandlesToBeVisible: false },
                { initText: "Hello cruel world", selectionStart: 2, selectionEnd: 9, expectHandlesToBeVisible: true },
                { initText: "Hello cruel world", selectionStart: 0, selectionEnd: 17, expectHandlesToBeVisible: true },
            ]
        }

        function test_selection(data) {
            waitForRendering(textInput)
            prepareTest(data)
            compare(inputPanel.cursorHandle.visible, data.expectHandlesToBeVisible)
            compare(inputPanel.anchorHandle.visible, data.expectHandlesToBeVisible)
            if (data.expectHandlesToBeVisible) {
                var cursorHandlePointsTo = Qt.point(inputPanel.cursorHandle.x + inputPanel.cursorHandle.width/2, inputPanel.cursorHandle.y)
                var anchorHandlePointsTo = Qt.point(inputPanel.anchorHandle.x + inputPanel.anchorHandle.width/2, inputPanel.anchorHandle.y)
                var anchorRect = textInput.positionToRectangle(data.selectionStart)
                var cursorRect = textInput.positionToRectangle(data.selectionEnd)

                compare(cursorHandlePointsTo.x, cursorRect.x)
                compare(cursorHandlePointsTo.y, cursorRect.y + cursorRect.height)

                compare(anchorHandlePointsTo.x, anchorRect.x)
                compare(anchorHandlePointsTo.y, anchorRect.y + anchorRect.height)
            }
        }
    }
}
