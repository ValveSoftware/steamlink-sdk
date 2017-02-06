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
        return Qt.createQmlObject('import QtQuick 2.0; import QtQuick.VirtualKeyboard 2.1; HangulInputMethod {}', parent, "hangulInputMethod")
    }
    sourceComponent: InputContext.shift ? page2 : page1
    sharedLayouts: ['symbols']
    Component {
        id: page1
        KeyboardLayout {
            keyWeight: 160
            KeyboardRow {
                Key {
                    text: "\u3142"
                }
                Key {
                    text: "\u3148"
                }
                Key {
                    text: "\u3137"
                }
                Key {
                    text: "\u3131"
                }
                Key {
                    text: "\u3145"
                }
                Key {
                    text: "\u315B"
                }
                Key {
                    text: "\u3155"
                }
                Key {
                    text: "\u3151"
                }
                Key {
                    text: "\u3150"
                }
                Key {
                    text: "\u3154"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 56
                }
                Key {
                    text: "\u3141"
                }
                Key {
                    text: "\u3134"
                }
                Key {
                    text: "\u3147"
                }
                Key {
                    text: "\u3139"
                }
                Key {
                    text: "\u314E"
                }
                Key {
                    text: "\u3157"
                }
                Key {
                    text: "\u3153"
                }
                Key {
                    text: "\u314F"
                }
                Key {
                    text: "\u3163"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                ShiftKey {}
                Key {
                    text: "\u314B"
                }
                Key {
                    text: "\u314C"
                }
                Key {
                    text: "\u314A"
                }
                Key {
                    text: "\u314D"
                }
                Key {
                    text: "\u3160"
                }
                Key {
                    text: "\u315C"
                }
                Key {
                    text: "\u3161"
                }
                Key {
                    key: Qt.Key_Comma
                    text: ","
                }
                Key {
                    key: Qt.Key_Period
                    text: "."
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
                    weight: 154
                }
                SpaceKey {
                    weight: 864
                }
                Key {
                    key: Qt.Key_Apostrophe
                    text: "'"
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
                    text: "\u3143"
                }
                Key {
                    text: "\u3149"
                }
                Key {
                    text: "\u3138"
                }
                Key {
                    text: "\u3132"
                }
                Key {
                    text: "\u3146"
                }
                Key {
                    text: "\u315B"
                }
                Key {
                    text: "\u3155"
                }
                Key {
                    text: "\u3151"
                }
                Key {
                    text: "\u3152"
                }
                Key {
                    text: "\u3156"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 56
                }
                Key {
                    text: "\u3141"
                }
                Key {
                    text: "\u3134"
                }
                Key {
                    text: "\u3147"
                }
                Key {
                    text: "\u3139"
                }
                Key {
                    text: "\u314E"
                }
                Key {
                    text: "\u3157"
                }
                Key {
                    text: "\u3153"
                }
                Key {
                    text: "\u314F"
                }
                Key {
                    text: "\u3163"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                ShiftKey {}
                Key {
                    text: "\u314B"
                }
                Key {
                    text: "\u314C"
                }
                Key {
                    text: "\u314A"
                }
                Key {
                    text: "\u314D"
                }
                Key {
                    text: "\u3160"
                }
                Key {
                    text: "\u315C"
                }
                Key {
                    text: "\u3161"
                }
                Key {
                    key: Qt.Key_Comma
                    text: ","
                }
                Key {
                    key: Qt.Key_Period
                    text: "."
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
                    weight: 154
                }
                SpaceKey {
                    weight: 864
                }
                Key {
                    key: Qt.Key_Apostrophe
                    text: "'"
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
