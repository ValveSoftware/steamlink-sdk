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

KeyboardLayout {
    inputMethod: PlainInputMethod {}
    inputMode: InputEngine.Numeric

    KeyboardColumn {
        Layout.fillWidth: false
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: height
        KeyboardRow {
            Key {
                key: 0x0667
                text: "\u0667"
                alternativeKeys: "\u06677"
            }
            Key {
                key: 0x0668
                text: "\u0668"
                alternativeKeys: "\u06688"
            }
            Key {
                key: 0x0669
                text: "\u0669"
                alternativeKeys: "\u06699"
            }
            BackspaceKey {}
        }
        KeyboardRow {
            Key {
                key: 0x0664
                text: "\u0664"
                alternativeKeys: "\u06644"
            }
            Key {
                key: 0x0665
                text: "\u0665"
                alternativeKeys: "\u06655"
            }
            Key {
                key: 0x0666
                text: "\u0666"
                alternativeKeys: "\u06666"
            }
            Key {
                text: " "
                displayText: "\u2423"
                repeat: true
                showPreview: false
                key: Qt.Key_Space
            }
        }
        KeyboardRow {
            Key {
                key: 0x0661
                text: "\u0661"
                alternativeKeys: "\u06611"
            }
            Key {
                key: 0x0662
                text: "\u0662"
                alternativeKeys: "\u06622"
            }
            Key {
                key: 0x0663
                text: "\u0663"
                alternativeKeys: "\u06633"
            }
            HideKeyboardKey {}
        }
        KeyboardRow {
            ChangeLanguageKey {
                customLayoutsOnly: true
            }
            Key {
                key: 0x0660
                text: "\u0660"
                alternativeKeys: "\u06600"
            }
            Key {
                key: Qt.Key_Comma
                text: "\u066B"
                alternativeKeys: "\u066B,."
            }
            EnterKey {}
        }
    }
}
