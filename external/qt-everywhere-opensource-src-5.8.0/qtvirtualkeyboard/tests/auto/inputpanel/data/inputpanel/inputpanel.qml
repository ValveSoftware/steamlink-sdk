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
import QtQuick.VirtualKeyboard 2.1
import QtQuick.VirtualKeyboard.Settings 2.1
import "handwriting.js" as Handwriting
import "utils.js" as Utils

InputPanel {
    id: inputPanel
    z: 99
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    visible: active

    property var testcase
    property var virtualKeyPressPoint: null
    readonly property int cursorPosition: InputContext.cursorPosition
    readonly property string preeditText: InputContext.preeditText
    readonly property string surroundingText: InputContext.surroundingText
    readonly property bool autoCapitalizationEnabled: InputContext.shiftHandler.autoCapitalizationEnabled
    readonly property bool toggleShiftEnabled: InputContext.shiftHandler.toggleShiftEnabled
    readonly property string locale: keyboard.locale
    readonly property string defaultLocale: VirtualKeyboardSettings.locale
    readonly property var availableLocales: VirtualKeyboardSettings.availableLocales
    readonly property var activeLocales: VirtualKeyboardSettings.activeLocales
    readonly property int inputMode: InputContext.inputEngine.inputMode
    readonly property var inputMethod: InputContext.inputEngine.inputMethod
    readonly property var keyboard: Utils.findChildByProperty(inputPanel, "objectName", "keyboard", null)
    readonly property bool handwritingMode: keyboard.handwritingMode
    readonly property var keyboardLayoutLoader: Utils.findChildByProperty(keyboard, "objectName", "keyboardLayoutLoader", null)
    readonly property var keyboardInputArea: Utils.findChildByProperty(keyboard, "objectName", "keyboardInputArea", null)
    readonly property var characterPreviewBubble: Utils.findChildByProperty(keyboard, "objectName", "characterPreviewBubble", null)
    readonly property var alternativeKeys: Utils.findChildByProperty(keyboard, "objectName", "alternativeKeys", null)
    readonly property var naviationHighlight: Utils.findChildByProperty(keyboard, "objectName", "naviationHighlight", null)
    readonly property bool naviationHighlightAnimating: naviationHighlight.xAnimation.running ||
                                                        naviationHighlight.yAnimation.running ||
                                                        naviationHighlight.widthAnimation.running ||
                                                        naviationHighlight.heightAnimation.running
    readonly property var wordCandidateView: Utils.findChildByProperty(keyboard, "objectName", "wordCandidateView", null)
    readonly property var selectionControl: Utils.findChild(inputPanel, null, function(obj, param) { return obj.hasOwnProperty("handleIsMoving")})
    readonly property var anchorHandle: selectionControl.children[0]
    readonly property var cursorHandle: selectionControl.children[1]
    readonly property bool wordCandidateListVisibleHint: InputContext.inputEngine.wordCandidateListVisibleHint
    readonly property bool keyboardLayoutsAvailable: keyboard.availableLocaleIndices.length > 0 && keyboard.availableLocaleIndices.indexOf(-1) === -1
    property alias keyboardLayoutsAvailableSpy: keyboardLayoutsAvailableSpy
    property alias keyboardLayoutLoaderItemSpy: keyboardLayoutLoaderItemSpy
    property alias characterPreviewBubbleSpy: characterPreviewBubbleSpy
    property alias alternativeKeysSpy: alternativeKeysSpy
    property alias activeKeyChangedSpy: activeKeyChangedSpy
    property alias virtualKeyClickedSpy: virtualKeyClickedSpy
    property alias dragSymbolModeSpy: dragSymbolModeSpy
    property alias styleSpy: styleSpy
    property alias soundEffectSpy: soundEffectSpy
    property alias inputMethodResultSpy: inputMethodResultSpy
    property alias wordCandidateListChangedSpy: wordCandidateListChangedSpy
    property alias shiftStateSpy: shiftStateSpy

    signal inputMethodResult(var text)

    Connections {
        target: InputContext
        onPreeditTextChanged: if (InputContext.preeditText.length > 0) inputMethodResult(InputContext.preeditText)
    }

    Connections {
        target: InputContext.inputEngine
        onVirtualKeyClicked: inputMethodResult(text)
    }

    SignalSpy {
        id: keyboardLayoutsAvailableSpy
        target: inputPanel
        signalName: "onKeyboardLayoutsAvailableChanged"
    }

    SignalSpy {
        id: keyboardLayoutLoaderItemSpy
        target: keyboardLayoutLoader
        signalName: "onItemChanged"
    }

    SignalSpy {
        id: characterPreviewBubbleSpy
        target: characterPreviewBubble
        signalName: "onVisibleChanged"
    }

    SignalSpy {
        id: alternativeKeysSpy
        target: alternativeKeys
        signalName: "onVisibleChanged"
    }

    SignalSpy {
        id: activeKeyChangedSpy
        target: InputContext.inputEngine
        signalName: "activeKeyChanged"
    }

    SignalSpy {
        id: virtualKeyClickedSpy
        target: InputContext.inputEngine
        signalName: "virtualKeyClicked"
    }

    SignalSpy {
        id: dragSymbolModeSpy
        target: keyboardInputArea
        signalName: "onDragSymbolModeChanged"
    }

    SignalSpy {
        id: styleSpy
        target: keyboard
        signalName: "onStyleChanged"
    }

    SignalSpy {
        id: soundEffectSpy
        target: keyboard.soundEffect
        signalName: "onPlayingChanged"
    }

    SignalSpy {
        id: inputMethodResultSpy
        target: inputPanel
        signalName: "inputMethodResult"
    }

    SignalSpy {
        id: wordCandidateListChangedSpy
        target: wordCandidateView.model
        signalName: "dataChanged"
    }

    SignalSpy {
        id: shiftStateSpy
        target: InputContext
        signalName: "onShiftChanged"
    }

    function findChildByProperty(parent, propertyName, propertyValue, compareCb) {
        var obj = null
        if (parent === null)
            return null
        var children = parent.children
        for (var i = 0; i < children.length; i++) {
            obj = children[i]
            if (obj.hasOwnProperty(propertyName)) {
                if (compareCb !== null) {
                    if (compareCb(obj[propertyName], propertyValue))
                        break
                } else if (obj[propertyName] === propertyValue) {
                    break
                }
            }
            obj = findChildByProperty(obj, propertyName, propertyValue, compareCb)
            if (obj)
                break
        }
        return obj
    }

    function isLocaleSupported(inputLocale) {
        var localeIndex = VirtualKeyboardSettings.availableLocales.indexOf(inputLocale)
        return localeIndex !== -1
    }

    function setLocale(inputLocale) {
        VirtualKeyboardSettings.locale = inputLocale
        if (["ar", "fa"].indexOf(inputLocale.substring(0, 2)) !== -1)
            return inputPanel.keyboard.locale.substring(0, 2) === inputLocale.substring(0, 2)
        return inputPanel.keyboard.locale === inputLocale
    }

    function setActiveLocales(activeLocales) {
        VirtualKeyboardSettings.activeLocales = activeLocales
    }

    function mapInputMode(inputModeName) {
        if (inputModeName === "Latin")
            return InputEngine.Latin
        else if (inputModeName === "Numeric")
            return InputEngine.Numeric
        else if (inputModeName === "Dialable")
            return InputEngine.Dialable
        else if (inputModeName === "Pinyin")
            return InputEngine.Pinyin
        else if (inputModeName === "Cangjie")
            return InputEngine.Cangjie
        else if (inputModeName === "Zhuyin")
            return InputEngine.Zhuyin
        else if (inputModeName === "Hangul")
            return InputEngine.Hangul
        else if (inputModeName === "Hiragana")
            return InputEngine.Hiragana
        else if (inputModeName === "Katakana")
            return InputEngine.Katakana
        else if (inputModeName === "FullwidthLatin")
            return InputEngine.FullwidthLatin
        else
            return -1
    }

    function isInputModeSupported(inputMode) {
        return InputContext.inputEngine.inputModes.indexOf(inputMode) !== -1
    }

    function setInputMode(inputMode) {
        if (!isInputModeSupported(inputMode))
            return false
        if (InputContext.inputEngine.inputMode !== inputMode)
            InputContext.inputEngine.inputMode = inputMode
        return true
    }

    function findVirtualKey(key) {
        return Utils.findChild(keyboardLayoutLoader, key, function(obj, param) {
            if (!obj.hasOwnProperty("key") || !obj.hasOwnProperty("text"))
                return false
            return (typeof param == "number") ? obj.key === param :  obj.text === param
        })
    }

    function findVirtualKeyAlternative(key) {
        if (typeof key != "string")
            return null
        return Utils.findChildByProperty(keyboardLayoutLoader, "effectiveAlternativeKeys", key, function(propertyValue, key) { return propertyValue.indexOf(key) !== -1 })
    }

    function virtualKeyPressOnCurrentLayout(key) {
        var keyObj = findVirtualKey(key)
        var alternativeKey = false
        if (!keyObj && typeof key == "string") {
            keyObj = findVirtualKeyAlternative(key)
            alternativeKey = keyObj !== null
            if (alternativeKey)
                alternativeKeysSpy.clear()
        }
        if (keyObj) {
            virtualKeyPressPoint = inputPanel.mapFromItem(keyObj, keyObj.width / 2, keyObj.height / 2)
            testcase.mousePress(inputPanel, virtualKeyPressPoint.x, virtualKeyPressPoint.y)
            testcase.wait(20)
            if (alternativeKey) {
                alternativeKeysSpy.wait()
                var keyIndex = keyObj.effectiveAlternativeKeys.indexOf(key)
                var itemX = keyIndex * keyboard.style.alternateKeysListItemWidth + keyboard.style.alternateKeysListItemWidth / 2
                virtualKeyPressPoint.x = inputPanel.mapFromItem(alternativeKeys.listView, itemX, 0).x
                testcase.mouseMove(inputPanel, virtualKeyPressPoint.x, virtualKeyPressPoint.y)
                testcase.waitForRendering(inputPanel)
            }
            return true
        }
        return false
    }

    function multiLayoutKeyActionHelper(key, keyActionOnCurrentLayoutCb) {
        if (!keyboardLayoutLoader.item) {
            console.warn("Key not found \\u%1 (keyboard layout not loaded)".arg(key.charCodeAt(0).toString(16)))
            return false
        }
        var success = keyActionOnCurrentLayoutCb(key)
        for (var c = 0; !success && c < 2; c++) {
            // Check if the current layout contains multiple layouts
            if (keyboardLayoutLoader.item.hasOwnProperty("item")) {
                if (keyboardLayoutLoader.item.hasOwnProperty("secondPage")) {
                    keyboardLayoutLoader.item.secondPage = !keyboardLayoutLoader.item.secondPage
                    testcase.waitForRendering(inputPanel)
                    success = keyActionOnCurrentLayoutCb(key)
                } else if (keyboardLayoutLoader.item.hasOwnProperty("page") && keyboardLayoutLoader.item.hasOwnProperty("numPages")) {
                    for (var page = 0; !success && page < keyboardLayoutLoader.item.numPages; page++) {
                        keyboardLayoutLoader.item.page = page
                        testcase.waitForRendering(inputPanel)
                        success = keyActionOnCurrentLayoutCb(key)
                    }
                } else {
                    // Some layouts (such as Arabic, Hindi) may have a second layout
                    InputContext.shiftHandler.toggleShift()
                    testcase.waitForRendering(inputPanel)
                    success = keyActionOnCurrentLayoutCb(key)
                    InputContext.shiftHandler.toggleShift()
                }
                if (success)
                    break
            }

            // Symbol mode not allowed in handwriting mode
            if (inputPanel.handwritingMode)
                break

            // Toggle symbol mode
            keyboard.symbolMode = !keyboard.symbolMode
            testcase.waitForRendering(inputPanel)
            success = keyActionOnCurrentLayoutCb(key)
        }
        if (!success)
            console.warn("Key not found \\u%1".arg(key.charCodeAt(0).toString(16)))
        return success
    }

    function virtualKeyPress(key) {
        return multiLayoutKeyActionHelper(key, virtualKeyPressOnCurrentLayout)
    }

    function virtualKeyDrag(key) {
        if (virtualKeyPressPoint !== null) {
            var keyObj = Utils.findChildByProperty(keyboardLayoutLoader, (typeof key == "number") ? "key" : "text", key, null)
            if (keyObj !== null) {
                virtualKeyPressPoint = inputPanel.mapFromItem(keyObj, keyObj.width / 2, keyObj.height / 2)
                testcase.mouseMove(inputPanel, virtualKeyPressPoint.x, virtualKeyPressPoint.y)
                testcase.waitForRendering(inputPanel)
                return true
            }
        }
        return false
    }

    function virtualKeyRelease() {
        if (virtualKeyPressPoint !== null) {
            testcase.mouseRelease(inputPanel, virtualKeyPressPoint.x, virtualKeyPressPoint.y)
            virtualKeyPressPoint = null
            return true
        }
        return false
    }

    function virtualKeyClick(key) {
        if (virtualKeyPress(key)) {
            virtualKeyRelease()
            testcase.waitForRendering(inputPanel)
            return true
        }
        return false
    }

    function emulateNavigationKeyClick(navigationKey) {
        testcase.keyClick(navigationKey)
        while (inputPanel.naviationHighlightAnimating)
            testcase.wait(inputPanel.naviationHighlight.moveDuration / 2)
    }

    function navigationHighlightContains(point) {
        var navigationPoint = inputPanel.mapToItem(inputPanel.naviationHighlight, point.x, point.y)
        return inputPanel.naviationHighlight.contains(Qt.point(navigationPoint.x, navigationPoint.y))
    }

    function navigateToKeyOnPoint(point) {
        activateNavigationKeyMode()
        if (inputPanel.naviationHighlight.visible) {
            while (true) {
                var navigationPoint = inputPanel.mapToItem(inputPanel.naviationHighlight, point.x, point.y)
                if (navigationHighlightContains(point))
                    return true
                if (inputPanel.naviationHighlight.y > point.y)
                    emulateNavigationKeyClick(Qt.Key_Up)
                else if (inputPanel.naviationHighlight.y + inputPanel.naviationHighlight.height < point.y)
                    emulateNavigationKeyClick(Qt.Key_Down)
                else if (inputPanel.naviationHighlight.x > point.x)
                    emulateNavigationKeyClick(Qt.Key_Left)
                else if (inputPanel.naviationHighlight.x + inputPanel.naviationHighlight.width < point.x)
                    emulateNavigationKeyClick(Qt.Key_Right)
            }
        }
        return false
    }

    function navigateToKeyOnCurrentLayout(key) {
        var keyObj = findVirtualKey(key)
        var alternativeKey = false
        if (!keyObj && typeof key == "string") {
            keyObj = findVirtualKeyAlternative(key)
            alternativeKey = keyObj !== null
        }
        if (keyObj) {
            var point = inputPanel.mapFromItem(keyObj, keyObj.width / 2, keyObj.height / 2)
            if (!navigateToKeyOnPoint(point))
                return false
            if (alternativeKey) {
                alternativeKeysSpy.clear()
                testcase.keyPress(Qt.Key_Return)
                alternativeKeysSpy.wait()
                testcase.keyRelease(Qt.Key_Return)
                var keyIndex = keyObj.effectiveAlternativeKeys.indexOf(key)
                while (inputPanel.alternativeKeys.listView.currentIndex !== keyIndex) {
                    testcase.verify(inputPanel.alternativeKeys.listView.currentIndex !== -1)
                    emulateNavigationKeyClick(inputPanel.alternativeKeys.listView.currentIndex < keyIndex ? Qt.Key_Right : Qt.Key_Left)
                }
            }
            return true
        }
        return false
    }

    function navigationKeyClickOnCurrentLayout(key) {
        if (navigateToKeyOnCurrentLayout(key)) {
            testcase.keyClick(Qt.Key_Return)
            return true
        }
        return false
    }

    function navigateToKey(key) {
        return multiLayoutKeyActionHelper(key, navigateToKeyOnCurrentLayout)
    }

    function navigationKeyClick(key) {
        return multiLayoutKeyActionHelper(key, navigationKeyClickOnCurrentLayout)
    }

    function activateNavigationKeyMode() {
        if (!inputPanel.naviationHighlight.visible) {
            emulateNavigationKeyClick(Qt.Key_Right)
            if (inputPanel.naviationHighlight.visible) {
                while (inputPanel.naviationHighlightAnimating)
                    testcase.wait(inputPanel.naviationHighlight.moveDuration / 2)
            }
        }
        return inputPanel.naviationHighlight.visible
    }

    function toggleShift() {
        InputContext.shiftHandler.toggleShift()
    }

    function style() {
        return VirtualKeyboardSettings.styleName
    }

    function setStyle(style) {
        VirtualKeyboardSettings.styleName = style
    }

    function selectionListSearchSuggestion(suggestion, timeout) {
        if (wordCandidateListVisibleHint === false)
            return false

        if (timeout !== undefined && timeout > 0) {
            // Note: Not using SignalSpy.wait() since it causes the test case to fail in case the signal is not emitted
            wordCandidateListChangedSpy.clear()
            var dt = new Date()
            var startTime = dt.getTime()
            while (wordCandidateListChangedSpy.count == 0) {
                dt = new Date()
                var elapsedTime = dt.getTime() - startTime
                if (elapsedTime >= timeout)
                    break
                var maxWait = Math.min(timeout - elapsedTime, 50)
                testcase.wait(maxWait)
            }
        }

        if (inputPanel.wordCandidateView.count === 0)
            return false;

        var suggestionFound = false
        var origIndex = inputPanel.wordCandidateView.currentIndex
        if (origIndex === -1) {
            inputPanel.wordCandidateView.incrementCurrentIndex()
            origIndex = inputPanel.wordCandidateView.currentIndex
        }
        if (origIndex !== -1) {
            while (true) {
                if (inputPanel.wordCandidateView.model.dataAt(inputPanel.wordCandidateView.currentIndex) === suggestion) {
                    suggestionFound = true
                    break
                }
                if (inputPanel.wordCandidateView.currentIndex === inputPanel.wordCandidateView.count - 1)
                    break
                inputPanel.wordCandidateView.incrementCurrentIndex()
            }
            if (!suggestionFound) {
                while (inputPanel.wordCandidateView.currentIndex !== origIndex) {
                    inputPanel.wordCandidateView.decrementCurrentIndex()
                }
            }
            testcase.waitForRendering(inputPanel)
        }
        return suggestionFound
    }

    function selectionListSelectCurrentItem() {
        if (!inputPanel.wordCandidateView.currentItem)
            return false
        testcase.wait(200)
        var itemPos = inputPanel.mapFromItem(inputPanel.wordCandidateView.currentItem,
                                             inputPanel.wordCandidateView.currentItem.width / 2,
                                             inputPanel.wordCandidateView.currentItem.height / 2)
        testcase.mouseClick(inputPanel, itemPos.x, itemPos.y, Qt.LeftButton, 0, 20)
        testcase.waitForRendering(inputPanel)
        return true
    }

    function setHandwritingMode(enabled) {
        if (inputPanel.keyboard.handwritingMode !== enabled) {
            if (!enabled || inputPanel.keyboard.isHandwritingAvailable())
                inputPanel.keyboard.handwritingMode = enabled
        }
        return inputPanel.keyboard.handwritingMode === enabled
    }

    function emulateHandwriting(ch, instant) {
        if (!inputPanel.keyboard.handwritingMode)
            return false
        var hwrInputArea = Utils.findChildByProperty(keyboard, "objectName", "hwrInputArea", null)
        inputMethodResultSpy.clear()
        if (!Handwriting.emulate(testcase, hwrInputArea, ch, instant)) {
            if (virtualKeyClick(ch))
                return true
            console.warn("Cannot produce the symbol '%1' in handwriting mode".arg(ch))
            return false
        }
        inputMethodResultSpy.wait(3000)
        return inputMethodResultSpy.count > 0
    }
}
