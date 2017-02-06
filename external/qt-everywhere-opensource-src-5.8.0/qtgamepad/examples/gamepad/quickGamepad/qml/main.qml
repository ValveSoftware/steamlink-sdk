/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Gamepad module
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

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtGamepad 1.0

ApplicationWindow {
    id: applicationWindow1
    visible: true
    width: 800
    height: 600
    title: qsTr("QtGamepad Example")

    Rectangle {
        id: background
        color: "#363330"
        anchors.fill: parent

        Item {
            id: buttonL2Item
            height: leftTrigger.height
            width: leftTrigger.width + buttonL2Value.width
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 8

            ButtonImage {
                id: leftTrigger
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                source: "xboxControllerLeftTrigger.png"
                active: gamepad.buttonL2 != 0
            }

            ProgressBar {
                id: buttonL2Value
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: leftTrigger.right
                orientation: 0
                value: gamepad.buttonL2
            }
        }
        ButtonImage {
            id: buttonL1
            anchors.left: buttonL2Item.left
            anchors.top: buttonL2Item.bottom
            anchors.topMargin: 8
            source: "xboxControllerLeftShoulder.png"
            active: gamepad.buttonL1
        }


        Item {
            id: buttonR2Item
            height: rightTrigger.height
            width: rightTrigger.width + buttonR2Value.width
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 8

            ButtonImage {
                id: rightTrigger
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                source: "xboxControllerRightTrigger.png"
                active: gamepad.buttonR2 != 0
            }

            ProgressBar {
                id: buttonR2Value
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: rightTrigger.left
                orientation: 0
                value: gamepad.buttonR2
            }
        }
        ButtonImage {
            id: buttonR1
            anchors.right: buttonR2Item.right
            anchors.top: buttonR2Item.bottom
            anchors.topMargin: 8
            source: "xboxControllerRightShoulder.png"
            active: gamepad.buttonR1
        }

        Item {
            id: centerButtons
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8
            height: guideButton.height
            width: guideButton.width + 16 + backButton.width + startButton.width
            ButtonImage {
                id: backButton
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: guideButton.left
                anchors.rightMargin: 8
                source: "xboxControllerBack.png"
                active: gamepad.buttonSelect
            }
            ButtonImage {
                id: guideButton
                anchors.centerIn: parent
                source: "xboxControllerButtonGuide.png"
                active: gamepad.buttonGuide
            }
            ButtonImage {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: guideButton.right
                anchors.leftMargin: 8
                id: startButton
                source: "xboxControllerStart.png"
                active: gamepad.buttonStart
            }
        }




        DPad {
            id: dPad
            gamepad: gamepad
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.margins: 8
            spacing: 16

            LeftThumbstick {
                id: leftThumbstick
                gamepad: gamepad
            }

            RightThumbstick {
                id: rightThumbstick
                gamepad: gamepad
            }
        }


        Item {
            width: 200
            height: 200
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            ButtonImage {
                id: buttonA
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                source: "xboxControllerButtonA.png";
                active: gamepad.buttonA
            }
            ButtonImage {
                id: buttonB
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                source: "xboxControllerButtonB.png";
                active: gamepad.buttonB
            }
            ButtonImage {
                id: buttonX
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                source: "xboxControllerButtonX.png";
                active: gamepad.buttonX
            }
            ButtonImage {
                id: buttonY
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                source: "xboxControllerButtonY.png";
                active: gamepad.buttonY
            }
        }

    }

    Connections {
        target: GamepadManager
        onGamepadConnected: gamepad.deviceId = deviceId
    }

    Gamepad {
        id: gamepad
        deviceId: GamepadManager.connectedGamepads.length > 0 ? GamepadManager.connectedGamepads[0] : -1
    }
}
