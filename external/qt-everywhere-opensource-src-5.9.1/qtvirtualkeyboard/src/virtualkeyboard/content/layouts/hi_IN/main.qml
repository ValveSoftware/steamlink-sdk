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
    inputMode: InputEngine.Latin
    sourceComponent: InputContext.shift ? page2 : page1
    Component {
        id: page1
        KeyboardLayout {
            keyWeight: 160
            KeyboardRow {
                Key {
                    text: "\u094C"
                }
                Key {
                    text: "\u0948"
                }
                Key {
                    text: "\u093E"
                }
                Key {
                    text: "\u0940"
                }
                Key {
                    text: "\u0942"
                }
                Key {
                    text: "\u092C"
                }
                Key {
                    text: "\u0939"
                }
                Key {
                    text: "\u0917"
                }
                Key {
                    text: "\u0926"
                }
                Key {
                    text: "\u091C"
                }
                Key {
                    text: "\u0921"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 66
                }
                Key {
                    text: "\u094B"
                    alternativeKeys: ["\u094B", "\u094A"]
                }
                Key {
                    text: "\u0947"
                }
                Key {
                    text: "\u094D"
                }
                Key {
                    text: "\u093F"
                }
                Key {
                    text: "\u0941"
                }
                Key {
                    text: "\u092A"
                }
                Key {
                    text: "\u0930"
                }
                Key {
                    text: "\u0915"
                }
                Key {
                    text: "\u0924"
                }
                Key {
                    text: "\u091A"
                }
                Key {
                    text: "\u091F"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                ShiftKey { }
                Key {
                    text: "\u0949"
                }
                Key {
                    text: "\u0902"
                    alternativeKeys: "\u0902\u0903"
                }
                Key {
                    text: "\u092E"
                }
                Key {
                    text: "\u0928"
                }
                Key {
                    text: "\u0935"
                }
                Key {
                    text: "\u0932"
                }
                Key {
                    text: "\u0938"
                }
                Key {
                    text: "\u092F"
                }
                Key {
                    text: "\u093C"
                }
                Key {
                    key: 0x2013
                    text: "\u2013"
                    alternativeKeys: "\u2013-“”"
                }
                ShiftKey {
                    weight: 264
                }
            }
            KeyboardRow {
                keyWeight: 154
                SymbolModeKey {
                    weight: 217
                    displayText: "&\u0967\u0968\u0969"
                }
                ChangeLanguageKey {
                    weight: 154
                }
                SpaceKey {
                    weight: 864
                }
                Key {
                    key: Qt.Key_Comma
                    text: ","
                    alternativeKeys: "!?:;.,|"
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
                    text: "\u0914"
                }
                Key {
                    text: "\u0910"
                }
                Key {
                    text: "\u0906"
                }
                Key {
                    text: "\u0908"
                }
                Key {
                    text: "\u090A"
                }
                Key {
                    text: "\u092D"
                }
                Key {
                    text: "\u0919"
                }
                Key {
                    text: "\u0918"
                }
                Key {
                    text: "\u0927"
                }
                Key {
                    text: "\u091D"
                }
                Key {
                    text: "\u0922"
                }
                BackspaceKey {}
            }
            KeyboardRow {
                FillerKey {
                    weight: 66
                }
                Key {
                    text: "\u0913"
                }
                Key {
                    text: "\u090F"
                }
                Key {
                    text: "\u0905"
                }
                Key {
                    text: "\u0907"
                }
                Key {
                    text: "\u0909"
                }
                Key {
                    text: "\u092B"
                }
                Key {
                    text: "\u0931"
                }
                Key {
                    text: "\u0916"
                }
                Key {
                    text: "\u0925"
                }
                Key {
                    text: "\u091B"
                }
                Key {
                    text: "\u0920"
                }
                EnterKey {
                    weight: 283
                }
            }
            KeyboardRow {
                keyWeight: 156
                ShiftKey { }
                Key {
                    text: "\u0911"
                }
                Key {
                    text: "\u0901"
                }
                Key {
                    text: "\u0923"
                }
                Key {
                    text: "\u0929"
                }
                Key {
                    text: "\u091E"
                }
                Key {
                    text: "\u0933"
                }
                Key {
                    text: "\u0936"
                }
                Key {
                    text: "\u0937"
                }
                Key {
                    text: "\u0943"
                }
                Key {
                    key: 0x2013
                    text: "\u2013"
                    alternativeKeys: "\u2013-“”"
                }
                ShiftKey {
                    weight: 264
                }
            }
            KeyboardRow {
                keyWeight: 154
                SymbolModeKey {
                    weight: 217
                    displayText: "&\u0967\u0968\u0969"
                }
                ChangeLanguageKey {
                    weight: 154
                }
                SpaceKey {
                    weight: 864
                }
                Key {
                    key: Qt.Key_Comma
                    text: ","
                    alternativeKeys: "!?:;.,|"
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
