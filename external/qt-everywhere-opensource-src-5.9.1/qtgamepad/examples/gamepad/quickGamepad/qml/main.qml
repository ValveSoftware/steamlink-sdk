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
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1
import QtQuick.Window 2.0
import QtGamepad 1.0

Window {
    id: applicationWindow1
    visible: true
    width: 800
    height: 600
    title: qsTr("QtGamepad Example")
    color: "#363330"

    Item {
        id: background
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            RowLayout {
                id: topRow
                width: parent.width
                ColumnLayout {
                    id: buttonL2Item
                    Layout.alignment: Qt.AlignLeft
                    ButtonImage {
                        id: leftTrigger
                        Layout.alignment: Qt.AlignCenter
                        source: "xboxControllerLeftTrigger.png"
                        active: gamepad.buttonL2 != 0
                    }
                    ProgressBar {
                        id: buttonL2Value
                        width: leftTrigger.width
                        value: gamepad.buttonL2
                    }
                }

                Item {
                    id: centerButtons
                    Layout.alignment: Qt.AlignCenter
                    Layout.fillWidth: true
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


                ColumnLayout {
                    id: buttonR2Item
                    Layout.alignment: Qt.AlignRight
                    ButtonImage {
                        id: rightTrigger
                        Layout.alignment: Qt.AlignCenter
                        source: "xboxControllerRightTrigger.png"
                        active: gamepad.buttonR2 != 0
                    }

                    ProgressBar {
                        id: buttonR2Value
                        width: rightTrigger.width
                        value: gamepad.buttonR2
                    }
                }
            }

            RowLayout {
                id: middleRow
                ButtonImage {
                    id: buttonL1
                    source: "xboxControllerLeftShoulder.png"
                    active: gamepad.buttonL1
                }
                Item {
                    id: spacer1
                    Layout.fillWidth: true
                }

                ButtonImage {
                    id: buttonR1
                    source: "xboxControllerRightShoulder.png"
                    active: gamepad.buttonR1
                }
            }

            RowLayout {
                id: bottomRow
                width: parent.width
                DPad {
                    id: dPad
                    Layout.alignment: Qt.AlignLeft
                    gamepad: gamepad
                }

                LeftThumbstick {
                    id: leftThumbstick
                    Layout.alignment: Qt.AlignLeft
                    gamepad: gamepad
                }

                Item {
                    id: spacer2
                    Layout.fillWidth: true
                }

                RightThumbstick {
                    id: rightThumbstick
                    Layout.alignment: Qt.AlignRight
                    gamepad: gamepad
                }

                Item {
                    width: 200
                    height: 200
                    Layout.alignment: Qt.AlignRight
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
