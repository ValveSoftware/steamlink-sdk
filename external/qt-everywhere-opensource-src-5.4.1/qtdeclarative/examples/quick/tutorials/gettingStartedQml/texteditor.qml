/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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
import "core"

Rectangle {
    id: screen
    width: 1000; height: 1000
    property int partition: height / 3
    state: "DRAWER_CLOSED"

    // Item 1: MenuBar on the top portion of the screen
    MenuBar {
        id: menuBar
        height: screen.partition
        width: screen.width
        z: 1
    }

    //Item 2: The editable text area
    TextArea {
        id: textArea
        y: drawer.height
        color: "#3F3F3F"
        fontColor: "#DCDCCC"
        height: partition * 2
        width: parent.width
    }

    // Item 3: The drawer handle
    Rectangle {
        id: drawer
        height:15; width: parent.width
        border.color : "#6A6D6A"
        border.width: 1
        z: 1

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#8C8F8C" }
            GradientStop { position: 0.17; color: "#6A6D6A" }
            GradientStop { position: 0.77; color: "#3F3F3F" }
            GradientStop { position: 1.0; color: "#6A6D6A" }
        }

        Image {
            id: arrowIcon
            source: "images/arrow.png"
            anchors.horizontalCenter: parent.horizontalCenter

            Behavior { NumberAnimation { property: "rotation"; easing.type: Easing.OutExpo } }
        }

        MouseArea {
            id: drawerMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: parent.border.color = Qt.lighter("#6A6D6A")
            onExited: parent.border.color = "#6A6D6A"
            onClicked: {
                if (screen.state == "DRAWER_CLOSED")
                    screen.state = "DRAWER_OPEN"
                else if (screen.state == "DRAWER_OPEN")
                    screen.state = "DRAWER_CLOSED"
            }
        }
    }

//! [states]
    states: [
        State {
            name: "DRAWER_OPEN"
            PropertyChanges { target: menuBar; y: 0 }
            PropertyChanges { target: textArea; y: partition + drawer.height }
            PropertyChanges { target: drawer; y: partition }
            PropertyChanges { target: arrowIcon; rotation: 180 }
        },
        State {
            name: "DRAWER_CLOSED"
            PropertyChanges { target: menuBar; y: -height; }
            PropertyChanges { target: textArea; y: drawer.height; height: screen.height - drawer.height }
            PropertyChanges { target: drawer; y: 0 }
            PropertyChanges { target: arrowIcon; rotation: 0 }
        }
    ]
//! [states]

//! [transitions]
    transitions: [
        Transition {
            to: "*"
            NumberAnimation { target: textArea; properties: "y, height"; duration: 100; easing.type: Easing.OutExpo }
            NumberAnimation { target: menuBar; properties: "y"; duration: 100; easing.type: Easing.OutExpo }
            NumberAnimation { target: drawer; properties: "y"; duration: 100; easing.type: Easing.OutExpo }
        }
    ]
//! [transitions]
}
