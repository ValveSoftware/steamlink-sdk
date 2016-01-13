/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.0
import "style"

Item {
    id: container

    signal clicked

    property alias text: buttonText.text
    property alias color: buttonText.color
    property alias paintedWidth: buttonText.paintedWidth
    property alias paintedHeight: buttonText.paintedHeight
    property bool checked: false
    property bool enabled: true
    property ButtonStyle style: ButtonStyle{}

    width: buttonText.width * 1.2
    height: buttonText.height * 1.2

    function disable() {
        enabled = false;
    }

    function enable() {
        enabled = true;
    }

    BorderImage {
        id: buttonImage
        source: container.style.background
        anchors.fill: parent
    }

    MouseArea {
        id: mouseRegion
        anchors.fill: buttonImage
        hoverEnabled: false
        onClicked: { container.clicked() }
    }
    Text {
        id: buttonText
        color: checked ? container.style.fontcolor_selected : container.style.fontcolor_normal
        anchors.centerIn: buttonImage; font.bold: true; font.pixelSize: 14
        style: Text.Normal
        anchors.baseline: parent.bottom
        anchors.baselineOffset: -6
    }

    states: [
        State {
            name: "Pressed"
            when: mouseRegion.pressed == true
            PropertyChanges { target: buttonImage; source: container.style.pressedBackground }
        },
        State {
            name: "Hovered"
            when: mouseRegion.containsMouse
            PropertyChanges{ target: buttonImage; source: container.style.disabledBackground }
        },
        State {
            name: "Disabled"
            when: !enabled
            PropertyChanges{ target: buttonText; color: "dimgray" }
            PropertyChanges{ target: mouseRegion; enabled: false }
        }
    ]
}
