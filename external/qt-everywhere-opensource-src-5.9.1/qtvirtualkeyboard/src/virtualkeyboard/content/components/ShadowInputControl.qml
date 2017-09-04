/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import QtQuick 2.7
import QtQuick.VirtualKeyboard 2.2
import QtQuick.VirtualKeyboard.Settings 2.2

Item {
    id: control

    enabled: keyboard.active && VirtualKeyboardSettings.fullScreenMode

    MouseArea {
        anchors.fill: parent
    }

    onXChanged: InputContext.shadow.updateSelectionProperties()
    onYChanged: InputContext.shadow.updateSelectionProperties()

    Loader {
        sourceComponent: keyboard.style.fullScreenInputContainerBackground
        anchors.fill: parent
        Loader {
            id: fullScreenInputBackground
            sourceComponent: keyboard.style.fullScreenInputBackground
            anchors.fill: parent
            anchors.margins: keyboard.style.fullScreenInputMargins
            z: 1
            Flickable {
                id: flickable
                clip: true
                z: 2
                width: parent.width
                height: parent.height
                flickableDirection: Flickable.HorizontalFlick
                interactive: contentWidth > width
                contentWidth: shadowInput.width
                onContentXChanged: InputContext.shadow.updateSelectionProperties()

                function ensureVisible(rectangle) {
                    if (contentX >= rectangle.x)
                        contentX = rectangle.x
                    else if (contentX + width <= rectangle.x + rectangle.width)
                        contentX = rectangle.x + rectangle.width - width;
                }

                TextInput {
                    id: shadowInput
                    objectName: "shadowInput"
                    property bool blinkStatus: true
                    width: Math.max(flickable.width, implicitWidth)
                    height: implicitHeight
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: keyboard.style.fullScreenInputPadding
                    rightPadding: keyboard.style.fullScreenInputPadding
                    activeFocusOnPress: false
                    font: keyboard.style.fullScreenInputFont
                    inputMethodHints: InputContext.inputMethodHints
                    cursorDelegate: keyboard.style.fullScreenInputCursor
                    passwordCharacter: keyboard.style.fullScreenInputPasswordCharacter
                    color: keyboard.style.fullScreenInputColor
                    selectionColor: keyboard.style.fullScreenInputSelectionColor
                    selectedTextColor: keyboard.style.fullScreenInputSelectedTextColor
                    echoMode: (InputContext.inputMethodHints & Qt.ImhHiddenText) ? TextInput.Password : TextInput.Normal
                    selectByMouse: true
                    onCursorPositionChanged: {
                        cursorSyncTimer.restart()
                        blinkStatus = true
                        cursorTimer.restart()
                    }
                    onSelectionStartChanged: cursorSyncTimer.restart()
                    onSelectionEndChanged: cursorSyncTimer.restart()
                    onCursorRectangleChanged: flickable.ensureVisible(cursorRectangle)

                    function getAnchorPosition() {
                        if (selectionStart == selectionEnd)
                            return cursorPosition
                        else if (selectionStart == cursorPosition)
                            return selectionEnd
                        else
                            return selectionStart
                    }

                    Timer {
                        id: cursorSyncTimer
                        interval: 0
                        onTriggered: {
                            var anchorPosition = shadowInput.getAnchorPosition()
                            if (anchorPosition !== InputContext.anchorPosition || shadowInput.cursorPosition !== InputContext.cursorPosition)
                                InputContext.forceCursorPosition(anchorPosition, shadowInput.cursorPosition)
                        }
                    }

                    Timer {
                        id: cursorTimer
                        interval: Qt.styleHints.cursorFlashTime / 2
                        repeat: true
                        running: true
                        onTriggered: shadowInput.blinkStatus = !shadowInput.blinkStatus
                    }
                }
            }
        }
    }

    Binding {
        target: InputContext.shadow
        property: "inputItem"
        value: shadowInput
        when: VirtualKeyboardSettings.fullScreenMode
    }
}
