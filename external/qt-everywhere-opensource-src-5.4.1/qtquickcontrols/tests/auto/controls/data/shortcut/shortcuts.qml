/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
import QtQuick.Controls 1.2

Rectangle {
    width: 300
    height: 300
    id: rect

    Text {
        id: text
        text: "hello"
        anchors.centerIn: parent
    }

    Action {
        shortcut: "a"
        onTriggered: text.text = "a pressed"
    }
    Action { // ambiguous but disabled
        enabled: false
        shortcut: "a"
        onTriggered: text.text = "a (disabled) pressed"
    }

    Action { // ambiguous but disabled
        enabled: false
        shortcut: "b"
        onTriggered: text.text = "b (disabled) pressed"
    }
    Action {
        shortcut: "b"
        onTriggered: text.text = "b pressed"
    }

    Action {
        shortcut: "ctrl+c"
        onTriggered: text.text = "ctrl c pressed"
    }

    Action {
        shortcut: "d"
        onTriggered: text.text = "d pressed"
    }

    Action {
        shortcut: "alt+d"
        onTriggered: text.text = "alt d pressed"
    }

    Action {
        shortcut: "shift+d"
        onTriggered: text.text = "shift d pressed 1"
    }
    Action {
        shortcut: "shift+d"
        onTriggered: text.text = "shift d pressed 2"
    }

    Action {
        text: "&test"
        onTriggered: text.text = "alt t pressed"
    }

    CheckBox {
        text: "&checkbox"
        onClicked: text.text = "alt c pressed"
    }

    RadioButton {
        text: "&radiobutton"
        onClicked: text.text = "alt r pressed"
    }

    Button {
        text: "button&1"
        onClicked: text.text = "alt 1 pressed"
    }

    Button {
        action: Action {
            text: "button&2"
        }
        onClicked: text.text = "alt 2 pressed"
    }

    ToolButton {
        text: "toolbutton&3"
        onClicked: text.text = "alt 3 pressed"
    }

    ToolButton {
        action: Action {
            text: "toolbutton&4"
        }
        onClicked: text.text = "alt 4 pressed"
    }
}
