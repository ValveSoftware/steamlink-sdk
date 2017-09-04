/***************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

import QtQuick 2.5

Item {
    id: root
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: 0.9
    }

    MouseArea {
        id: eventEater
    }

    Rectangle {
        id: dialogFrame

        anchors.centerIn: parent
        width: parent.width * 0.8
        height: parent.height * 0.6
        border.color: "#454545"
        color: GameSettings.backgroundColor
        radius: width * 0.05

        Item {
            id: dialogContainer
            anchors.fill: parent
            anchors.margins: parent.width*0.05

            Image {
                id: offOnImage
                anchors.left: quitButton.left
                anchors.right: quitButton.right
                anchors.top: parent.top
                height: GameSettings.heightForWidth(width, sourceSize)
                source: "images/bt_off_to_on.png"
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: offOnImage.bottom
                anchors.bottom: quitButton.top
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                font.pixelSize: GameSettings.mediumFontSize
                color: GameSettings.textColor
                text: qsTr("This application cannot be used without Bluetooth. Please switch Bluetooth ON to continue.")
            }

            GameButton {
                id: quitButton
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: dialogContainer.width * 0.6
                height: GameSettings.buttonHeight
                onClicked: Qt.quit()

                Text {
                    anchors.centerIn: parent
                    color: GameSettings.textColor
                    font.pixelSize: GameSettings.bigFontSize
                    text: qsTr("Quit")
                }
            }
        }
    }
}

