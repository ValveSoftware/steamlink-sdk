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

import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.VirtualKeyboard 2.1
import QtQuick.VirtualKeyboard.Styles 2.1
import QtQuick.VirtualKeyboard.Settings 2.1
import Qt.labs.folderlistmodel 2.0

Item {
    id: keyboard
    objectName: "keyboard"

    property alias style: styleLoader.item
    property var activeKey: null
    property TouchPoint activeTouchPoint
    property int localeIndex: -1
    property var availableLocaleIndices: []
    property var availableCustomLocaleIndices: []
    property string locale: localeIndex >= 0 && localeIndex < layoutsModel.count ? layoutsModel.get(localeIndex, "fileName") : ""
    property string inputLocale
    property int defaultLocaleIndex: -1
    property bool latinOnly: InputContext.inputMethodHints & Qt.ImhLatinOnly
    property bool preferNumbers: InputContext.inputMethodHints & Qt.ImhPreferNumbers
    property string layout
    property string layoutType: {
        if (keyboard.handwritingMode) return "handwriting"
        if (InputContext.inputMethodHints & Qt.ImhDialableCharactersOnly) return "dialpad"
        if (InputContext.inputMethodHints & Qt.ImhFormattedNumbersOnly) return "numbers"
        if (InputContext.inputMethodHints & Qt.ImhDigitsOnly) return "digits"
        if (keyboard.symbolMode) return "symbols"
        return "main"
    }
    property bool active: Qt.inputMethod.visible
    property bool uppercased: InputContext.shift
    property bool handwritingMode
    property bool fullScreenHandwritingMode
    property bool symbolMode
    property var defaultInputMethod: initDefaultInputMethod()
    property var plainInputMethod: PlainInputMethod {}
    property var customInputMethod: null
    property var customInputMethodSharedLayouts: []
    property int defaultInputMode: InputEngine.Latin
    property bool inputMethodNeedsReset: true
    property bool inputModeNeedsReset: true
    property bool navigationModeActive: false
    property alias soundEffect: soundEffect

    function initDefaultInputMethod() {
        try {
            return Qt.createQmlObject('import QtQuick 2.0; import QtQuick.VirtualKeyboard 2.1; HunspellInputMethod {}', keyboard, "defaultInputMethod")
        } catch (e) { }
        return plainInputMethod
    }

    width: keyboardBackground.width
    height: wordCandidateView.height + keyboardBackground.height
    onActiveChanged: {
        keyboardInputArea.reset()
    }
    onActiveKeyChanged: {
        if (InputContext.inputEngine.activeKey !== Qt.Key_unknown)
            InputContext.inputEngine.virtualKeyCancel()
    }
    Connections {
        target: VirtualKeyboardSettings
        onLocaleChanged: {
            updateDefaultLocale()
            localeIndex = defaultLocaleIndex
        }
        onActiveLocalesChanged: {
            updateDefaultLocale()
            if (!isValidLocale(localeIndex) || VirtualKeyboardSettings.locale)
                localeIndex = defaultLocaleIndex
        }
    }
    onLocaleChanged: {
        inputMethodNeedsReset = true
        inputModeNeedsReset = true
        updateLayout()
    }
    onInputLocaleChanged: {
        if (Qt.locale(inputLocale).name !== "C")
            InputContext.locale = inputLocale
    }
    onLayoutTypeChanged: {
        updateAvailableLocaleIndices()
        updateLayout()
    }
    onLatinOnlyChanged: {
        if (!latinOnly)
            inputModeNeedsReset = true
        updateInputMethod()
    }
    onPreferNumbersChanged: {
        keyboard.symbolMode = !keyboard.handwritingMode && preferNumbers
        if (!preferNumbers)
            inputModeNeedsReset = true
        updateInputMethod()
    }
    onHandwritingModeChanged: if (!keyboard.handwritingMode) keyboard.fullScreenHandwritingMode = false
    onFullScreenHandwritingModeChanged: if (keyboard.fullScreenHandwritingMode) keyboard.handwritingMode = true

    Connections {
        target: InputContext
        onFocusChanged: {
            if (InputContext.focus)
                updateInputMethod()
        }
        onInputMethodHintsChanged: {
            if (InputContext.focus)
                updateInputMethod()
        }
        onNavigationKeyPressed: {
            var initialKey
            switch (key) {
            case Qt.Key_Left:
                if (keyboard.navigationModeActive && !keyboardInputArea.initialKey) {
                    if (alternativeKeys.active) {
                        if (alternativeKeys.listView.currentIndex > 0) {
                            alternativeKeys.listView.decrementCurrentIndex()
                        } else {
                            alternativeKeys.close()
                            keyboardInputArea.setActiveKey(null)
                            keyboardInputArea.navigateToNextKey(0, 0, false)
                        }
                        break
                    }
                    if (wordCandidateView.count) {
                        if (wordCandidateView.currentIndex > 0) {
                            wordCandidateView.decrementCurrentIndex()
                        } else {
                            keyboardInputArea.navigateToNextKey(0, 0, false)
                            initialKey = keyboardInputArea.initialKey
                            if (!keyboardInputArea.navigateToNextKey(-1, -1, false)) {
                                keyboardInputArea.initialKey = initialKey
                                keyboardInputArea.navigateToNextKey(-1, -1, true)
                            } else {
                                keyboardInputArea.navigateToNextKey(1, 1, false)
                            }
                        }
                        break
                    }
                }
                initialKey = keyboardInputArea.initialKey
                if (!keyboardInputArea.navigateToNextKey(-1, 0, false)) {
                    keyboardInputArea.initialKey = initialKey
                    if (!keyboardInputArea.navigateToNextKey(0, -1, false)) {
                        if (wordCandidateView.count) {
                            if (wordCandidateView.currentIndex === -1)
                                wordCandidateView.incrementCurrentIndex()
                            break
                        }
                        keyboardInputArea.initialKey = initialKey
                        keyboardInputArea.navigateToNextKey(0, -1, true)
                    }
                    keyboardInputArea.navigateToNextKey(-1, 0, true)
                }
                break
            case Qt.Key_Up:
                if (alternativeKeys.active) {
                    alternativeKeys.close()
                    keyboardInputArea.setActiveKey(null)
                    keyboardInputArea.navigateToNextKey(0, 0, false)
                } else if (keyboard.navigationModeActive && !keyboardInputArea.initialKey && wordCandidateView.count) {
                    keyboardInputArea.navigateToNextKey(0, 0, false)
                    initialKey = keyboardInputArea.initialKey
                    if (!keyboardInputArea.navigateToNextKey(0, -1, false)) {
                        keyboardInputArea.initialKey = initialKey
                        keyboardInputArea.navigateToNextKey(0, -1, true)
                    } else {
                        keyboardInputArea.navigateToNextKey(0, 1, false)
                    }
                } else if (!keyboardInputArea.navigateToNextKey(0, -1, !keyboard.navigationModeActive || !keyboardInputArea.initialKey || wordCandidateView.count == 0)) {
                    if (wordCandidateView.currentIndex === -1)
                        wordCandidateView.incrementCurrentIndex()
                }
                break
            case Qt.Key_Right:
                if (keyboard.navigationModeActive && !keyboardInputArea.initialKey) {
                    if (alternativeKeys.active) {
                        if (alternativeKeys.listView.currentIndex + 1 < alternativeKeys.listView.count) {
                            alternativeKeys.listView.incrementCurrentIndex()
                        } else {
                            alternativeKeys.close()
                            keyboardInputArea.setActiveKey(null)
                            keyboardInputArea.navigateToNextKey(0, 0, false)
                        }
                        break
                    }
                    if (wordCandidateView.count) {
                        if (wordCandidateView.currentIndex + 1 < wordCandidateView.count) {
                            wordCandidateView.incrementCurrentIndex()
                        } else {
                            keyboardInputArea.navigateToNextKey(0, 0, false)
                            initialKey = keyboardInputArea.initialKey
                            if (!keyboardInputArea.navigateToNextKey(1, 1, false)) {
                                keyboardInputArea.initialKey = initialKey
                                keyboardInputArea.navigateToNextKey(1, 1, true)
                            } else {
                                keyboardInputArea.navigateToNextKey(-1, -1, false)
                            }
                        }
                        break
                    }
                }
                initialKey = keyboardInputArea.initialKey
                if (!keyboardInputArea.navigateToNextKey(1, 0, false)) {
                    keyboardInputArea.initialKey = initialKey
                    if (!keyboardInputArea.navigateToNextKey(0, 1, false)) {
                        if (wordCandidateView.count) {
                            if (wordCandidateView.currentIndex === -1)
                                wordCandidateView.incrementCurrentIndex()
                            break
                        }
                        keyboardInputArea.initialKey = initialKey
                        keyboardInputArea.navigateToNextKey(0, 1, true)
                    }
                    keyboardInputArea.navigateToNextKey(1, 0, true)
                }
                break
            case Qt.Key_Down:
                if (alternativeKeys.active) {
                    alternativeKeys.close()
                    keyboardInputArea.setActiveKey(null)
                    keyboardInputArea.navigateToNextKey(0, 0, false)
                } else if (keyboard.navigationModeActive && !keyboardInputArea.initialKey && wordCandidateView.count) {
                    keyboardInputArea.navigateToNextKey(0, 0, false)
                    initialKey = keyboardInputArea.initialKey
                    if (!keyboardInputArea.navigateToNextKey(0, 1, false)) {
                        keyboardInputArea.initialKey = initialKey
                        keyboardInputArea.navigateToNextKey(0, 1, true)
                    } else {
                        keyboardInputArea.navigateToNextKey(0, -1, false)
                    }
                } else if (!keyboardInputArea.navigateToNextKey(0, 1, !keyboard.navigationModeActive || !keyboardInputArea.initialKey || wordCandidateView.count == 0)) {
                    if (wordCandidateView.currentIndex === -1)
                        wordCandidateView.incrementCurrentIndex()
                }
                break
            case Qt.Key_Return:
                if (!keyboard.navigationModeActive)
                    break
                if (alternativeKeys.active) {
                    if (!isAutoRepeat) {
                        alternativeKeys.clicked()
                        keyboardInputArea.reset()
                        keyboardInputArea.navigateToNextKey(0, 0, false)
                    }
                } else if (keyboardInputArea.initialKey) {
                    if (!isAutoRepeat) {
                        pressAndHoldTimer.restart()
                        keyboardInputArea.setActiveKey(keyboardInputArea.initialKey)
                        keyboardInputArea.press(keyboardInputArea.initialKey, true)
                    }
                } else if (wordCandidateView.count > 0) {
                    wordCandidateView.model.selectItem(wordCandidateView.currentIndex)
                    if (!InputContext.preeditText.length)
                        keyboardInputArea.navigateToNextKey(0, 1, true)
                }
                break
            default:
                break
            }
        }
        onNavigationKeyReleased: {
            switch (key) {
            case Qt.Key_Return:
                if (!keyboard.navigationModeActive)
                    break
                if (!alternativeKeys.active && keyboard.activeKey && !isAutoRepeat) {
                    keyboardInputArea.release(keyboard.activeKey)
                    pressAndHoldTimer.stop()
                    alternativeKeys.close()
                    keyboardInputArea.setActiveKey(null)
                    if (keyboardInputArea.navigationCursor !== Qt.point(-1, -1))
                        keyboardInputArea.navigateToNextKey(0, 0, false)
                }
                break
            default:
                break
            }
        }
    }
    Connections {
        target: InputContext.inputEngine
        onVirtualKeyClicked: {
            if (isAutoRepeat && keyboard.activeKey)
                soundEffect.play(keyboard.activeKey.soundEffect)
            if (key !== Qt.Key_unknown && keyboardInputArea.dragSymbolMode) {
                keyboardInputArea.dragSymbolMode = false
                keyboard.symbolMode = false
            } else if (key === Qt.Key_Space) {
                var surroundingText = InputContext.surroundingText.trim()
                if (InputContext.shiftHandler.sentenceEndingCharacters.indexOf(surroundingText.charAt(surroundingText.length-1)) >= 0)
                    keyboard.symbolMode = false
            }
        }
    }
    FolderListModel {
        id: layoutsModel
        nameFilters: ["$"]
        Component.onCompleted: {
            if (InputContext.fileExists("qrc:/QtQuick/VirtualKeyboard/content/layouts"))
                layoutsModel.folder = "qrc:/QtQuick/VirtualKeyboard/content/layouts"
            else
                layoutsModel.folder = "../layouts"
        }
    }
    Connections {
        target: layoutsModel
        onCountChanged: {
            updateDefaultLocale()
            localeIndex = defaultLocaleIndex
        }
    }
    AlternativeKeys {
        id: alternativeKeys
        objectName: "alternativeKeys"
        // Add some extra margin for decoration
        property real horizontalMargin: style.alternateKeysListItemWidth
        property real verticalMargin: style.alternateKeysListItemHeight
        property rect previewRect: Qt.rect(keyboard.x + alternativeKeys.listView.x - horizontalMargin,
                                           keyboard.y + alternativeKeys.listView.y - verticalMargin,
                                           alternativeKeys.listView.width + horizontalMargin * 2,
                                           alternativeKeys.listView.height + verticalMargin * 2)
        onVisibleChanged: {
            if (visible)
                InputContext.previewRectangle = Qt.binding(function() {return previewRect})
            InputContext.previewVisible = visible
        }
    }
    Timer {
        id: pressAndHoldTimer
        interval: 800
        onTriggered: {
            if (keyboard.activeKey && keyboard.activeKey === keyboardInputArea.initialKey) {
                var origin = keyboard.mapFromItem(activeKey, activeKey.width / 2, 0)
                if (alternativeKeys.open(keyboard.activeKey, origin.x, origin.y)) {
                    InputContext.inputEngine.virtualKeyCancel()
                    keyboardInputArea.initialKey = null
                } else if (keyboard.activeKey.key === Qt.Key_Context1) {
                    InputContext.inputEngine.virtualKeyCancel()
                    keyboardInputArea.dragSymbolMode = true
                    keyboard.symbolMode = true
                    keyboardInputArea.initialKey = null
                    if (keyboardInputArea.navigationCursor !== Qt.point(-1, -1))
                        keyboardInputArea.navigateToNextKey(0, 0, false)
                }
            } else if (keyboardInputArea.dragSymbolMode &&
                       keyboard.activeKey &&
                       keyboard.activeKey.functionKey &&
                       !keyboard.activeKey.repeat) {
                InputContext.inputEngine.virtualKeyCancel()
                keyboardInputArea.click(keyboard.activeKey)
                keyboardInputArea.initialKey = null
                if (keyboardInputArea.navigationCursor !== Qt.point(-1, -1))
                    keyboardInputArea.navigateToNextKey(0, 0, false)
            }
        }
    }
    Timer {
        id: releaseInaccuracyTimer
        interval: 500
        onTriggered: {
            if (keyboardInputArea.pressed && activeTouchPoint && !alternativeKeys.active && !keyboardInputArea.dragSymbolMode) {
                var key = keyboardInputArea.keyOnPoint(activeTouchPoint.x, activeTouchPoint.y)
                if (key !== keyboard.activeKey) {
                    InputContext.inputEngine.virtualKeyCancel()
                    keyboardInputArea.setActiveKey(key)
                    keyboardInputArea.press(key, false)
                }
            }
        }
    }
    CharacterPreviewBubble {
        id: characterPreview
        objectName: "characterPreviewBubble"
        active: keyboardInputArea.pressed && !alternativeKeys.active
        property rect previewRect: Qt.rect(keyboard.x + characterPreview.x,
                                           keyboard.y + characterPreview.y,
                                           characterPreview.width,
                                           characterPreview.height)
        onVisibleChanged: {
            if (visible)
                InputContext.previewRectangle = Qt.binding(function() {return previewRect})
            InputContext.previewVisible = visible
        }
    }
    Binding {
        target: InputContext
        property: "keyboardRectangle"
        value: Qt.rect(keyboard.x, keyboard.y, keyboard.width, keyboard.height)
        when: keyboard.active && !InputContext.animating
    }
    Loader {
        id: styleLoader
        source: VirtualKeyboardSettings.style
        Binding {
            target: styleLoader.item
            property: "keyboardHeight"
            value: keyboardInnerContainer.height
        }
    }
    Loader {
        id: naviationHighlight
        objectName: "naviationHighlight"
        property var highlightItem: {
            if (keyboard.navigationModeActive) {
                if (keyboardInputArea.initialKey) {
                    return keyboardInputArea.initialKey
                } else if (alternativeKeys.listView.count > 0) {
                    return alternativeKeys.listView.highlightItem
                } else if (wordCandidateView.count > 0) {
                    return wordCandidateView.highlightItem
                }
            }
            return keyboard
        }
        // Note: without "highlightItem.x - highlightItem.x" the binding does not work for alternativeKeys
        property var highlightItemOffset: highlightItem ? keyboard.mapFromItem(highlightItem, highlightItem.x - highlightItem.x, highlightItem.y - highlightItem.y) : ({x:0, y:0})
        property int moveDuration: 200
        property int resizeDuration: 200
        property alias xAnimation: xAnimation
        property alias yAnimation: yAnimation
        property alias widthAnimation: widthAnimation
        property alias heightAnimation: heightAnimation
        z: 2
        x: highlightItemOffset.x
        y: highlightItemOffset.y
        width: highlightItem ? highlightItem.width : 0
        height: highlightItem ? highlightItem.height : 0
        visible: keyboard.navigationModeActive && highlightItem !== null && highlightItem !== keyboard
        sourceComponent: keyboard.style.navigationHighlight
        Behavior on x {
            NumberAnimation { id: xAnimation; duration: naviationHighlight.moveDuration; easing.type: Easing.OutCubic }
        }
        Behavior on y {
            NumberAnimation { id: yAnimation; duration: naviationHighlight.moveDuration; easing.type: Easing.OutCubic }
        }
        Behavior on width {
            NumberAnimation { id: widthAnimation; duration: naviationHighlight.resizeDuration; easing.type: Easing.OutCubic }
        }
        Behavior on height {
            NumberAnimation { id: heightAnimation; duration: naviationHighlight.resizeDuration; easing.type: Easing.OutCubic }
        }
    }

    ListView {
        id: wordCandidateView
        objectName: "wordCandidateView"
        clip: true
        height: Math.round(style.selectionListHeight)
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        spacing: 0
        orientation: ListView.Horizontal
        snapMode: ListView.SnapToItem
        delegate: style.selectionListDelegate
        highlight: style.selectionListHighlight ? style.selectionListHighlight : defaultHighlight
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        add: style.selectionListAdd
        remove: style.selectionListRemove
        keyNavigationWraps: true
        model: InputContext.inputEngine.wordCandidateListModel
        onCurrentItemChanged: if (currentItem) soundEffect.register(currentItem.soundEffect)
        Connections {
            target: wordCandidateView.model ? wordCandidateView.model : null
            onActiveItemChanged: wordCandidateView.currentIndex = index
            onItemSelected: if (wordCandidateView.currentItem) soundEffect.play(wordCandidateView.currentItem.soundEffect)
        }
        Loader {
            sourceComponent: style.selectionListBackground
            anchors.fill: parent
            z: -1
        }
        Component {
            id: defaultHighlight
            Item {}
        }
    }

    Item {
        id: soundEffect
        property var __sounds: ({})
        property bool available: false

        signal playingChanged(url source, bool playing)

        Connections {
            target: VirtualKeyboardSettings
            onStyleNameChanged: {
                soundEffect.__sounds = {}
                soundEffect.available = false
            }
        }

        function play(sound) {
            if (enabled && sound != Qt.resolvedUrl("")) {
                var soundId = Qt.md5(sound)
                var multiSoundEffect = __sounds[soundId]
                if (!multiSoundEffect)
                    multiSoundEffect = register(sound)
                if (multiSoundEffect)
                    multiSoundEffect.play()
            }
        }

        function register(sound) {
            var multiSoundEffect = null
            if (enabled && sound != Qt.resolvedUrl("")) {
                var soundId = Qt.md5(sound)
                multiSoundEffect = __sounds[soundId]
                if (!multiSoundEffect) {
                    multiSoundEffect = Qt.createQmlObject('import QtQuick 2.0; import QtQuick.VirtualKeyboard 2.1; MultiSoundEffect {}', soundEffect)
                    if (multiSoundEffect) {
                        multiSoundEffect.playingChanged.connect(soundEffect.playingChanged)
                        multiSoundEffect.source = sound
                        __sounds[soundId] = multiSoundEffect
                        available = true
                    }
                }
            }
            return multiSoundEffect
        }
    }

    Loader {
        id: keyboardBackground
        z: -1
        anchors.left: parent.left
        anchors.top: wordCandidateView.bottom
        anchors.right: parent.right
        height: keyboardInnerContainer.height
        sourceComponent: style.keyboardBackground

        Item {
            id: keyboardInnerContainer
            z: 1
            width: Math.round(keyboardBackground.width)
            height: Math.round(style.keyboardDesignHeight * width / style.keyboardDesignWidth)
            anchors.horizontalCenter: parent.horizontalCenter

            Loader {
                id: keyboardLayoutLoader
                objectName: "keyboardLayoutLoader"

                anchors.fill: parent
                anchors.leftMargin: Math.round(style.keyboardRelativeLeftMargin * parent.width)
                anchors.rightMargin: Math.round(style.keyboardRelativeRightMargin * parent.width)
                anchors.topMargin: Math.round(style.keyboardRelativeTopMargin * parent.height)
                anchors.bottomMargin: Math.round(style.keyboardRelativeBottomMargin * parent.height)

                Binding {
                    target: keyboardLayoutLoader
                    property: "source"
                    value: keyboard.layout
                    when: keyboard.layout.length > 0
                }

                MultiPointTouchArea {
                    id: keyboardInputArea
                    objectName: "keyboardInputArea"

                    property var initialKey: null
                    property bool dragSymbolMode
                    property real releaseMargin: initialKey !== null ? Math.min(initialKey.width / 3, initialKey.height / 3) : 0
                    property point navigationCursor: Qt.point(-1, -1)

                    anchors.fill: keyboardLayoutLoader

                    Connections {
                        target: keyboardLayoutLoader
                        onStatusChanged: {
                            if (keyboardLayoutLoader.status == Loader.Ready &&
                                    keyboard.navigationModeActive &&
                                    keyboardInputArea.navigationCursor !== Qt.point(-1, -1))
                                keyboard.navigationModeActive = keyboardInputArea.navigateToNextKey(0, 0, false)
                        }
                    }
                    Connections {
                        target: keyboard
                        onNavigationModeActiveChanged: {
                            if (!keyboard.navigationModeActive) {
                                keyboardInputArea.navigationCursor = Qt.point(-1, -1)
                                keyboardInputArea.reset()
                            }
                        }
                    }

                    function press(key, isRealPress) {
                        if (key && key.enabled) {
                            if (!key.noKeyEvent)
                                InputContext.inputEngine.virtualKeyPress(key.key, key.uppercased ? key.text.toUpperCase() : key.text, key.uppercased ? Qt.ShiftModifier : 0, key.repeat && !dragSymbolMode)
                            if (isRealPress)
                                soundEffect.play(key.soundEffect)
                        }
                    }
                    function release(key) {
                        if (key && key.enabled) {
                            if (!key.noKeyEvent)
                                InputContext.inputEngine.virtualKeyRelease(key.key, key.uppercased ? key.text.toUpperCase() : key.text, key.uppercased ? Qt.ShiftModifier : 0)
                            key.clicked()
                        }
                    }
                    function click(key) {
                        if (key && key.enabled) {
                            if (!key.noKeyEvent)
                                InputContext.inputEngine.virtualKeyClick(key.key, keyboard.uppercased ? key.text.toUpperCase() : key.text, keyboard.uppercased ? Qt.ShiftModifier : 0)
                            key.clicked()
                        }
                    }
                    function setActiveKey(activeKey) {
                        if (keyboard.activeKey === activeKey)
                            return
                        if (keyboard.activeKey) {
                            keyboard.activeKey.active = false
                        }
                        keyboard.activeKey = activeKey
                        if (keyboard.activeKey) {
                            keyboard.activeKey.active = true
                        }
                    }
                    function keyOnPoint(px, py) {
                        var parentItem = keyboardLayoutLoader
                        var child = parentItem.childAt(px, py)
                        while (child !== null) {
                            var position = parentItem.mapToItem(child, px, py)
                            px = position.x; py = position.y
                            parentItem = child
                            child = parentItem.childAt(px, py)
                            if (child && child.key !== undefined)
                                return child
                        }
                        return null
                    }
                    function hitInitialKey(x, y, margin) {
                        if (!initialKey)
                            return false
                        var position = initialKey.mapFromItem(keyboardInputArea, x, y)
                        return (position.x > -margin
                                && position.y > -margin
                                && position.x < initialKey.width + margin
                                && position.y < initialKey.height + margin)
                    }
                    function containsPoint(touchPoints, point) {
                        if (!point)
                            return false
                        for (var i in touchPoints)
                            if (touchPoints[i].pointId == point.pointId)
                                return true
                        return false
                    }
                    function releaseActiveKey() {
                        if (alternativeKeys.active) {
                            alternativeKeys.clicked()
                        } else if (keyboard.activeKey) {
                            release(keyboard.activeKey)
                        }
                        reset()
                    }
                    function reset() {
                        releaseInaccuracyTimer.stop()
                        pressAndHoldTimer.stop()
                        setActiveKey(null)
                        activeTouchPoint = null
                        alternativeKeys.close()
                        if (dragSymbolMode) {
                            keyboard.symbolMode = false
                            dragSymbolMode = false
                        }
                    }
                    function nextKeyInNavigation(dX, dY, wrapEnabled) {
                        var nextKey = null, x, y, itemOffset
                        if (dX !== 0 || dY !== 0) {
                            var offsetX, offsetY
                            for (offsetX = dX, offsetY = dY;
                                 Math.abs(offsetX) < width && Math.abs(offsetY) < height;
                                 offsetX += dX, offsetY += dY) {
                                x = navigationCursor.x + offsetX
                                if (x < 0) {
                                    if (!wrapEnabled)
                                        break
                                    x += width
                                } else if (x >= width) {
                                    if (!wrapEnabled)
                                        break
                                    x -= width
                                }
                                y = navigationCursor.y + offsetY
                                if (y < 0) {
                                    if (!wrapEnabled)
                                        break
                                    y += height
                                } else if (y >= height) {
                                    if (!wrapEnabled)
                                        break
                                    y -= height
                                }
                                nextKey = keyOnPoint(x, y)
                                if (nextKey) {
                                    // Check if key is visible. Only the visible keys have keyPanelDelegate set.
                                    if (nextKey != initialKey && nextKey.hasOwnProperty("keyPanelDelegate") && nextKey.keyPanelDelegate)
                                        break
                                    // Jump over the item to reduce the number of iterations in this loop
                                    itemOffset = mapToItem(nextKey, x, y)
                                    if (dX > 0)
                                        offsetX += nextKey.width - itemOffset.x
                                    else if (dX < 0)
                                        offsetX -= itemOffset.x
                                    else if (dY > 0)
                                        offsetY += nextKey.height - itemOffset.y
                                    else if (dY < 0)
                                        offsetY -= itemOffset.y
                                }
                                nextKey = null
                            }
                        } else {
                            nextKey = keyOnPoint(navigationCursor.x, navigationCursor.y)
                        }
                        if (nextKey) {
                            itemOffset = mapFromItem(nextKey, nextKey.width / 2, nextKey.height / 2)
                            if (dX) {
                                x = itemOffset.x
                            } else if (dY) {
                                y = itemOffset.y
                            } else {
                                x = itemOffset.x
                                y = itemOffset.y
                            }
                            navigationCursor = Qt.point(x, y)
                        }
                        return nextKey
                    }
                    function navigateToNextKey(dX, dY, wrapEnabled) {
                        // Resolve initial landing point of the navigation cursor
                        if (!keyboard.navigationModeActive || keyboard.navigationCursor === Qt.point(-1, -1)) {
                            if (dX > 0)
                                navigationCursor = Qt.point(0, height / 2)
                            else if (dX < 0)
                                navigationCursor = Qt.point(width, height / 2)
                            else if (dY > 0)
                                navigationCursor = Qt.point(width / 2, 0)
                            else if (dY < 0)
                                navigationCursor = Qt.point(width / 2, height)
                            else
                                navigationCursor = Qt.point(width / 2, height / 2)
                            keyboard.navigationModeActive = true
                        }
                        if (dX && dY) {
                            initialKey = nextKeyInNavigation(dX, 0, wrapEnabled)
                            if (initialKey || wrapEnabled)
                                initialKey = nextKeyInNavigation(0, dY, wrapEnabled)
                        } else {
                            initialKey = nextKeyInNavigation(dX, dY, wrapEnabled)
                        }
                        return initialKey !== null
                    }

                    onPressed: {
                        keyboard.navigationModeActive = false

                        // Immediately release any pending key that the user might be
                        // holding (and about to release) when a second key is pressed.
                        if (activeTouchPoint)
                            releaseActiveKey();

                        for (var i in touchPoints) {
                            // Release any key pressed by a previous iteration of the loop.
                            if (containsPoint(touchPoints, activeTouchPoint))
                                releaseActiveKey();

                            releaseInaccuracyTimer.start()
                            pressAndHoldTimer.start()
                            initialKey = keyOnPoint(touchPoints[i].x, touchPoints[i].y)
                            activeTouchPoint = touchPoints[i]
                            setActiveKey(initialKey)
                            press(initialKey, true)
                        }
                    }
                    onUpdated: {
                        if (!containsPoint(touchPoints, activeTouchPoint))
                            return

                        if (alternativeKeys.active) {
                            alternativeKeys.move(mapToItem(alternativeKeys, activeTouchPoint.x, 0).x)
                        } else {
                            var key = null
                            if (releaseInaccuracyTimer.running) {
                                if (hitInitialKey(activeTouchPoint.x, activeTouchPoint.y, releaseMargin)) {
                                    key = initialKey
                                } else if (initialKey) {
                                    releaseInaccuracyTimer.stop()
                                    initialKey = null
                                }
                            }
                            if (key === null) {
                                key = keyOnPoint(activeTouchPoint.x, activeTouchPoint.y)
                            }
                            if (key !== keyboard.activeKey) {
                                InputContext.inputEngine.virtualKeyCancel()
                                setActiveKey(key)
                                press(key, false)
                                if (dragSymbolMode)
                                    pressAndHoldTimer.restart()
                            }
                        }
                    }
                    onReleased: {
                        if (containsPoint(touchPoints, activeTouchPoint))
                            releaseActiveKey();
                    }
                    onCanceled: {
                        if (containsPoint(touchPoints, activeTouchPoint))
                            reset()
                    }
                }
            }
        }
    }

    function updateInputMethod() {
        if (!keyboardLayoutLoader.item)
            return
        if (!InputContext.focus)
            return

        // Reset the custom input method if it is not included in the list of shared layouts
        if (customInputMethod && !inputMethodNeedsReset && customInputMethodSharedLayouts.indexOf(layoutType) === -1)
            inputMethodNeedsReset = true

        if (inputMethodNeedsReset) {
            if (customInputMethod) {
                customInputMethod.destroy()
                customInputMethod = null
            }
            customInputMethodSharedLayouts = []
            inputMethodNeedsReset = false
        }

        var inputMethod = null
        var inputMode = InputContext.inputEngine.inputMode

        // Use input method from keyboard layout
        if (keyboardLayoutLoader.item.inputMethod) {
            inputMethod = keyboardLayoutLoader.item.inputMethod
        } else if (!customInputMethod) {
            try {
                customInputMethod = keyboardLayoutLoader.item.createInputMethod()
                if (customInputMethod) {
                    // Pull the list of shared layouts from the keyboard layout
                    if (keyboardLayoutLoader.item.sharedLayouts)
                        customInputMethodSharedLayouts = customInputMethodSharedLayouts.concat(keyboardLayoutLoader.item.sharedLayouts)

                    // Make sure the current layout is included in the list
                    if (customInputMethodSharedLayouts.indexOf(layoutType) === -1)
                        customInputMethodSharedLayouts.push(layoutType)
                }
            } catch (e) {
                console.error(e.message)
            }
        }
        if (!inputMethod)
            inputMethod = customInputMethod ? customInputMethod : defaultInputMethod

        var inputMethodChanged = InputContext.inputEngine.inputMethod !== inputMethod
        if (inputMethodChanged) {
            InputContext.inputEngine.inputMethod = inputMethod
        }

        if (InputContext.inputEngine.inputMethod) {
            var inputModes = InputContext.inputEngine.inputModes
            if (inputModes.length > 0) {
                // Reset to default input mode if the input locale has changed
                if (inputModeNeedsReset)
                    inputMode = inputModes[0]

                // Check the current layout for input mode override
                if (keyboardLayoutLoader.item.inputMode !== -1)
                    inputMode = keyboardLayoutLoader.item.inputMode

                // Update input mode automatically in handwriting mode
                if (keyboard.handwritingMode) {
                    if ((InputContext.inputMethodHints & Qt.ImhDialableCharactersOnly) && inputModes.indexOf(InputEngine.Dialable) !== -1)
                        inputMode = InputEngine.Dialable
                    else if ((InputContext.inputMethodHints & (Qt.ImhFormattedNumbersOnly | Qt.ImhDigitsOnly)) && inputModes.indexOf(InputEngine.Numeric) !== -1)
                        inputMode = InputEngine.Numeric
                    else
                        inputMode = inputModes[0]
                }

                // Check the input method hints for input mode overrides
                if (latinOnly)
                    inputMode = InputEngine.Latin
                if (preferNumbers)
                    inputMode = InputEngine.Numeric

                // Make sure the input mode is supported by the current input method
                if (inputModes.indexOf(inputMode) === -1)
                    inputMode = inputModes[0]

                if (InputContext.inputEngine.inputMode !== inputMode || inputMethodChanged || inputModeNeedsReset)
                    InputContext.inputEngine.inputMode = inputMode

                inputModeNeedsReset = false
            }
        }
    }

    function updateLayout() {
        var newLayout
        newLayout = findLayout(locale, layoutType)
        if (!newLayout.length) {
            newLayout = findLayout(locale, "main")
        }
        layout = newLayout
        inputLocale = locale
        updateInputMethod()
    }

    function updateDefaultLocale() {
        updateAvailableLocaleIndices()
        if (layoutsModel.count > 0) {
            var defaultLocales = []
            if (isValidLocale(VirtualKeyboardSettings.locale))
                defaultLocales.push(VirtualKeyboardSettings.locale)
            if (isValidLocale(InputContext.locale))
                defaultLocales.push(InputContext.locale)
            if (VirtualKeyboardSettings.activeLocales.length > 0 && isValidLocale(VirtualKeyboardSettings.activeLocales[0]))
                defaultLocales.push(VirtualKeyboardSettings.activeLocales[0])
            if (VirtualKeyboardSettings.availableLocales.indexOf("en_GB") !== -1)
                defaultLocales.push("en_GB")
            if (availableLocaleIndices.length > 0)
                defaultLocales.push(layoutsModel.get(availableLocaleIndices[0], "fileName"))
            var newDefaultLocaleIndex = -1
            for (var i = 0; i < defaultLocales.length; i++) {
                newDefaultLocaleIndex = findLocale(defaultLocales[i], -1)
                if (availableLocaleIndices.indexOf(newDefaultLocaleIndex) !== -1)
                    break;
                newDefaultLocaleIndex = -1
            }
            defaultLocaleIndex = newDefaultLocaleIndex
        } else {
            defaultLocaleIndex = -1
        }
    }

    function updateAvailableLocaleIndices() {
        // Update list of all available locales
        var baseLayoutIndex = findLocale("en_GB", -1)
        var newIndices = []
        var newAvailableLocales = []
        for (var i = 0; i < layoutsModel.count; i++) {
            var localeName = layoutsModel.get(i, "fileName")
            if (isValidLocale(i) && newIndices.indexOf(i) === -1 && findLayout(localeName, "main")) {
                newIndices.push(i)
                newAvailableLocales.push(localeName)
            }
        }

        // Handle case where the VirtualKeyboardSettings.activeLocales contains no valid entries
        if (newIndices.length === 0) {
            if (baseLayoutIndex !== -1) {
                newIndices.push(baseLayoutIndex)
                newAvailableLocales.push("en_GB")
            } else {
                for (i = 0; i < layoutsModel.count; i++) {
                    localeName = layoutsModel.get(i, "fileName")
                    if (Qt.locale(localeName).name !== "C" && findLayout(localeName, "main")) {
                        newIndices.push(i)
                        newAvailableLocales.push(localeName)
                        break
                    }
                }
            }
        }

        newIndices.sort(function(a, b) { return a - b })
        availableLocaleIndices = newIndices
        newAvailableLocales.sort()
        InputContext.updateAvailableLocales(newAvailableLocales)

        // Update list of custom locale indices
        newIndices = []
        for (i = 0; i < availableLocaleIndices.length; i++) {
            if (availableLocaleIndices[i] === localeIndex ||
                    ((availableLocaleIndices[i] !== baseLayoutIndex ||
                      (layoutType === "handwriting" && availableLocaleIndices.indexOf(baseLayoutIndex) !== -1)) &&
                     layoutExists(layoutsModel.get(availableLocaleIndices[i], "fileName"), layoutType)))
                newIndices.push(availableLocaleIndices[i])
        }
        availableCustomLocaleIndices = newIndices
    }

    function nextLocaleIndex(customLayoutsOnly) {
        var newLocaleIndex = localeIndex
        var localeIndices = customLayoutsOnly ? availableCustomLocaleIndices : availableLocaleIndices
        var i = localeIndices.indexOf(localeIndex)
        if (i !== -1) {
            i = (i + 1) % localeIndices.length
            newLocaleIndex = localeIndices[i]
        }
        return newLocaleIndex
    }

    function changeInputLanguage(customLayoutsOnly) {
        var newLocaleIndex = nextLocaleIndex(customLayoutsOnly)
        if (newLocaleIndex !== -1 && newLocaleIndex !== localeIndex)
            localeIndex = newLocaleIndex
    }

    function canChangeInputLanguage(customLayoutsOnly) {
        if (customLayoutsOnly)
            return availableCustomLocaleIndices.length > 1
        return availableLocaleIndices.length > 1
    }

    function findLocale(localeName, defaultValue) {
        var languageCode = localeName.substring(0, 3) // Including the '_' delimiter
        var languageMatch = -1
        for (var i = 0; i < layoutsModel.count; i++) {
            if (!layoutsModel.isFolder(i))
                continue
            var layoutFolder = layoutsModel.get(i, "fileName")
            if (layoutFolder === localeName)
                return i
            if (languageMatch == -1 && layoutFolder.substring(0, 3) === languageCode)
                languageMatch = i
        }
        return (languageMatch != -1) ? languageMatch : defaultValue
    }

    function isValidLocale(localeNameOrIndex) {
        var localeName
        if (typeof localeNameOrIndex == "number") {
            if (localeNameOrIndex < 0 || localeNameOrIndex >= layoutsModel.count)
                return false
            localeName = layoutsModel.get(localeNameOrIndex, "fileName")
        } else {
            localeName = localeNameOrIndex
        }

        if (Qt.locale(localeName).name === "C")
            return false

        if (VirtualKeyboardSettings.activeLocales.length > 0 && VirtualKeyboardSettings.activeLocales.indexOf(localeName) === -1)
            return false

        return true
    }

    function getLayoutFile(localeName, layoutType) {
        if (localeName === "" || layoutType === "")
            return ""
        return layoutsModel.folder + "/" + localeName + "/" + layoutType + ".qml"
    }

    function layoutExists(localeName, layoutType) {
        return InputContext.fileExists(getLayoutFile(localeName, layoutType))
    }

    function findLayout(localeName, layoutType) {
        var layoutFile = getLayoutFile(localeName, layoutType)
        if (InputContext.fileExists(layoutFile))
            return layoutFile
        layoutFile = getLayoutFile("en_GB", layoutType)
        if (InputContext.fileExists(layoutFile))
            return layoutFile
        return ""
    }

    function isHandwritingAvailable() {
        return VirtualKeyboardInputMethods.indexOf("HandwritingInputMethod") !== -1 && layoutExists(locale, "handwriting")
    }
}
