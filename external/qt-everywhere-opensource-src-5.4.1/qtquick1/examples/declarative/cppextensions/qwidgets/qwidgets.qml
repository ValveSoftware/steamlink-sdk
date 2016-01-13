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

import QtQuick 1.0
import "QWidgets" 1.0

Rectangle {
    id: window

    property int margin: 30

    width: 640; height: 480
    color: palette.window

    SystemPalette { id: palette }

    MyPushButton {
        id: button1
        x: margin; y: margin
        text: "Right"
        transformOriginPoint: Qt.point(width / 2, height / 2)

        onClicked: window.state = "right"
    }

    MyPushButton {
        id: button2
        x: margin; y: margin + 30
        text: "Bottom"
        transformOriginPoint: Qt.point(width / 2, height / 2)

        onClicked: window.state = "bottom"
    }

    MyPushButton {
        id: button3
        x: margin; y: margin + 60
        text: "Quit"
        transformOriginPoint: Qt.point(width / 2, height / 2)

        onClicked: Qt.quit()
    }

    states: [
        State {
            name: "right"
            PropertyChanges { target: button1; x: window.width - width - margin; text: "Left"; onClicked: window.state = "" }
            PropertyChanges { target: button2; x: window.width - width - margin }
            PropertyChanges { target: button3; x: window.width - width - margin }
            PropertyChanges { target: window; color: Qt.darker(palette.window) }
        },
        State {
            name: "bottom"
            PropertyChanges { target: button1; y: window.height - height - margin; rotation: 180 }
            PropertyChanges {
                target: button2
                x: button1.x + button1.width + 10; y: window.height - height - margin
                rotation: 180
                text: "Top"
                onClicked: window.state = ""
            }
            PropertyChanges { target: button3; x: button2.x + button2.width + 10; y: window.height - height - margin;  rotation: 180 }
            PropertyChanges { target: window; color: Qt.lighter(palette.window) }
        }
    ]

    transitions: Transition {
        ParallelAnimation {
            NumberAnimation { properties: "x,y,rotation"; duration: 600; easing.type: Easing.OutQuad }
            ColorAnimation { target: window; duration: 600 }
        }
    }
}
