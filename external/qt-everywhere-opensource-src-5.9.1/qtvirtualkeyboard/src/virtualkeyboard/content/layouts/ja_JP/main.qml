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
import QtQuick.VirtualKeyboard 2.1

KeyboardLayoutLoader {
    function createInputMethod() {
        return Qt.createQmlObject('import QtQuick 2.0; import QtQuick.VirtualKeyboard 2.1; JapaneseInputMethod {}', parent, "japaneseInputMethod")
    }
    sharedLayouts: ['symbols']
    sourceComponent: InputContext.inputEngine.inputMode === InputEngine.FullwidthLatin ? page2 : page1
    Component {
        id: page1
        KeyboardLayout {
            keyWeight: 160
            KeyboardRow {
                Key {
                    key: Qt.Key_Q
                    text: "q"
                }
                Key {
                    key: Qt.Key_W
                    text: "w"
                }
                Key {
                    key: Qt.Key_E
                    text: "e"
                }
                Key {
                    key: Qt.Key_R
                    text: "r"
                }
                Key {
                    key: Qt.Key_T
                    text: "t"
                }
                Key {
                    key: Qt.Key_Y
                    text: "y"
                }
                Key {
                    key: Qt.Key_U
                    text: "u"
                }
                Key {
                    key: Qt.Key_I
                    text: "i"
                }
                Key {
                    key: Qt.Key_O
                    text: "o"
                }
                Key {
                    key: Qt.Key_P
                    text: "p"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 56
                }
                Key {
                    key: Qt.Key_A
                    text: "a"
                }
                Key {
                    key: Qt.Key_S
                    text: "s"
                }
                Key {
                    key: Qt.Key_D
                    text: "d"
                }
                Key {
                    key: Qt.Key_F
                    text: "f"
                }
                Key {
                    key: Qt.Key_G
                    text: "g"
                }
                Key {
                    key: Qt.Key_H
                    text: "h"
                }
                Key {
                    key: Qt.Key_J
                    text: "j"
                }
                Key {
                    key: Qt.Key_K
                    text: "k"
                }
                Key {
                    key: Qt.Key_L
                    text: "l"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                Key {
                    key: Qt.Key_Mode_switch
                    noKeyEvent: true
                    functionKey: true
                    enabled: !(InputContext.inputMethodHints & Qt.ImhLatinOnly)
                    displayText: {
                        switch (InputContext.inputEngine.inputMode) {
                        case InputEngine.Latin:
                            return "\u534A\u89D2"
                        case InputEngine.Hiragana:
                            return "\u3042"
                        case InputEngine.Katakana:
                            return "\u30AB"
                        default:
                            return ""
                        }
                    }
                    onClicked: {
                        switch (InputContext.inputEngine.inputMode) {
                        case InputEngine.Latin:
                            InputContext.inputEngine.inputMode = InputEngine.FullwidthLatin
                            break
                        case InputEngine.Hiragana:
                            InputContext.inputEngine.inputMode = InputEngine.Katakana
                            break
                        case InputEngine.Katakana:
                            InputContext.inputEngine.inputMode = InputEngine.Latin
                            break
                        default:
                            break
                        }
                    }
                }
                Key {
                    key: Qt.Key_Z
                    text: "z"
                }
                Key {
                    key: Qt.Key_X
                    text: "x"
                }
                Key {
                    key: Qt.Key_C
                    text: "c"
                }
                Key {
                    key: Qt.Key_V
                    text: "v"
                }
                Key {
                    key: Qt.Key_B
                    text: "b"
                }
                Key {
                    key: Qt.Key_N
                    text: "n"
                }
                Key {
                    key: Qt.Key_M
                    text: "m"
                }
                Key {
                    key: Qt.Key_Comma
                    text: "\u3001"
                    alternativeKeys: "\u3001\uFF01,!"
                }
                Key {
                    key: Qt.Key_Period
                    text: "\u3002"
                    alternativeKeys: "\u3002\uFF1F.?"
                }
                ShiftKey {
                    weight: 204
                }
            }
            KeyboardRow {
                keyWeight: 154
                SymbolModeKey {
                    weight: 217
                }
                ChangeLanguageKey {
                    weight: 174
                }
                Key {
                    key: Qt.Key_Left
                    displayText: "\u2190"
                    repeat: true
                    noModifier: true
                    functionKey: true
                }
                Key {
                    key: Qt.Key_Right
                    displayText: "\u2192"
                    repeat: true
                    noModifier: true
                    functionKey: true
                }
                SpaceKey {
                    weight: 556
                    text: InputContext.inputEngine.inputMode != InputEngine.Latin ? "\u3000" : " "
                }
                Key {
                    key: Qt.Key_Slash
                    text: "\u30FB"
                    alternativeKeys: "\u30FB/"
                }
                Key {
                    key: Qt.Key_Apostrophe
                    text: "\uFF07"
                    alternativeKeys: "\uFF07\uFF02'\""
                }
                Key {
                    key: Qt.Key_Underscore
                    text: "\u30FC"
                    alternativeKeys: "\u30FC\uFF5E\uFF70\uFF3F_"
                }
                Key {
                    key: 0xE000
                    text: ":-)"
                    alternativeKeys: [ ";-)", ":-)", ":-D", ":-(", "<3" ]
                }
                HideKeyboardKey {
                    weight: 204
                }
            }
        }
    }
    Component {
        id: page2
        KeyboardLayout {
            keyWeight: 160
            KeyboardRow {
                Key {
                    key: Qt.Key_Q
                    text: "\uFF51"
                }
                Key {
                    key: Qt.Key_W
                    text: "\uFF57"
                }
                Key {
                    key: Qt.Key_E
                    text: "\uFF45"
                }
                Key {
                    key: Qt.Key_R
                    text: "\uFF52"
                }
                Key {
                    key: Qt.Key_T
                    text: "\uFF54"
                }
                Key {
                    key: Qt.Key_Y
                    text: "\uFF59"
                }
                Key {
                    key: Qt.Key_U
                    text: "\uFF55"
                }
                Key {
                    key: Qt.Key_I
                    text: "\uFF49"
                }
                Key {
                    key: Qt.Key_O
                    text: "\uFF4F"
                }
                Key {
                    key: Qt.Key_P
                    text: "\uFF50"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 56
                }
                Key {
                    key: Qt.Key_A
                    text: "\uFF41"
                }
                Key {
                    key: Qt.Key_S
                    text: "\uFF53"
                }
                Key {
                    key: Qt.Key_D
                    text: "\uFF44"
                }
                Key {
                    key: Qt.Key_F
                    text: "\uFF46"
                }
                Key {
                    key: Qt.Key_G
                    text: "\uFF47"
                }
                Key {
                    key: Qt.Key_H
                    text: "\uFF48"
                }
                Key {
                    key: Qt.Key_J
                    text: "\uFF4A"
                }
                Key {
                    key: Qt.Key_K
                    text: "\uFF4B"
                }
                Key {
                    key: Qt.Key_L
                    text: "\uFF4C"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                Key {
                    key: Qt.Key_Mode_switch
                    noKeyEvent: true
                    functionKey: true
                    displayText: "\u5168\u89D2"
                    onClicked: InputContext.inputEngine.inputMode = InputEngine.Hiragana
                }
                Key {
                    key: Qt.Key_Z
                    text: "\uFF5A"
                }
                Key {
                    key: Qt.Key_X
                    text: "\uFF58"
                }
                Key {
                    key: Qt.Key_C
                    text: "\uFF43"
                }
                Key {
                    key: Qt.Key_V
                    text: "\uFF56"
                }
                Key {
                    key: Qt.Key_B
                    text: "\uFF42"
                }
                Key {
                    key: Qt.Key_N
                    text: "\uFF4E"
                }
                Key {
                    key: Qt.Key_M
                    text: "\uFF4D"
                }
                Key {
                    key: Qt.Key_Comma
                    text: "\u3001"
                    alternativeKeys: "\u3001\uFF01,!"
                }
                Key {
                    key: Qt.Key_Period
                    text: "\u3002"
                    alternativeKeys: "\u3002\uFF1F.?"
                }
                ShiftKey {
                    weight: 204
                }
            }
            KeyboardRow {
                keyWeight: 154
                SymbolModeKey {
                    weight: 217
                }
                ChangeLanguageKey {
                    weight: 174
                }
                Key {
                    key: Qt.Key_Left
                    displayText: "\u2190"
                    repeat: true
                    noModifier: true
                    functionKey: true
                }
                Key {
                    key: Qt.Key_Right
                    displayText: "\u2192"
                    repeat: true
                    noModifier: true
                    functionKey: true
                }
                SpaceKey {
                    weight: 556
                    text: "\u3000"
                }
                Key {
                    key: Qt.Key_Slash
                    text: "\u30FB"
                    alternativeKeys: "\u30FB/"
                }
                Key {
                    key: Qt.Key_Apostrophe
                    text: "\uFF07"
                    alternativeKeys: "\uFF07\uFF02'\""
                }
                Key {
                    key: Qt.Key_Underscore
                    text: "\u30FC"
                    alternativeKeys: "\u30FC\uFF5E\uFF70\uFF3F_"
                }
                Key {
                    key: 0xE000
                    text: ":-)"
                    alternativeKeys: [ ";-)", ":-)", ":-D", ":-(", "<3" ]
                }
                HideKeyboardKey {
                    weight: 204
                }
            }
        }
    }
}
