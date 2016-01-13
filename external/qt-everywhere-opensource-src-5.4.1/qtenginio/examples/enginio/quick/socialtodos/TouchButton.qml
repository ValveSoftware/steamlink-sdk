/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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
import QtQuick 2.1

Item {
    id: button
    signal clicked
    property alias text: label.text
    property color baseColor: "#555"
    property color textColor: "#fff"
    height: Math.round(40 * scaleFactor)
    width: Math.round(160 * scaleFactor)
    activeFocusOnTab: true


    Rectangle {
        anchors.fill: button
        color: "#ffffff"
        anchors.bottomMargin: intScaleFactor
        radius: background.radius
    }

    Keys.onReturnPressed: clicked()

    Rectangle {
        id: background
        opacity: enabled ? 1 : 0.7
        Behavior on opacity {NumberAnimation{}}
        radius: height/2
        border.width: intScaleFactor * button.activeFocus ? 2 : 1
        border.color: Qt.darker(baseColor, 1.4)
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { color: mousearea.pressed ? baseColor : Qt.lighter(baseColor, 1.4) ; position: 0 }
            GradientStop { color: baseColor ; position: 1 }
        }

        Text {
            id: label
            anchors.centerIn: parent
            font.pixelSize: 22 * scaleFactor
            font.bold: true
            color: textColor
            style: Text.Raised
            styleColor: "#44000000"
        }

        MouseArea {
            id: mousearea
            onClicked: button.clicked()
            anchors.fill: parent
        }
    }
}
