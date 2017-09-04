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

GamePage {
    id: measurePage

    errorMessage: deviceHandler.error
    infoMessage: deviceHandler.info

    property real __timeCounter: 0;
    property real __maxTimeCount: 60
    property string relaxText: qsTr("Relax!\nWhen you are ready, press Start. You have %1s time to increase heartrate so much as possible.\nGood luck!").arg(__maxTimeCount)

    function close()
    {
        deviceHandler.stopMeasurement();
        deviceHandler.disconnectService();
        app.prevPage();
    }

    function start()
    {
        if (!deviceHandler.measuring) {
            __timeCounter = 0;
            deviceHandler.startMeasurement()
        }
    }

    function stop()
    {
        if (deviceHandler.measuring) {
            deviceHandler.stopMeasurement()
        }

        app.showPage("Stats.qml")
    }

    Timer {
        id: measureTimer
        interval: 1000
        running: deviceHandler.measuring
        repeat: true
        onTriggered: {
            __timeCounter++;
            if (__timeCounter >= __maxTimeCount)
                measurePage.stop()
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: GameSettings.fieldHeight * 0.5

        Rectangle {
            id: circle
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(measurePage.width, measurePage.height-GameSettings.fieldHeight*4) - 2*GameSettings.fieldMargin
            height: width
            radius: width*0.5
            color: GameSettings.viewColor

            Text {
                id: hintText
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -parent.height*0.1
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                width: parent.width * 0.8
                height: parent.height * 0.6
                wrapMode: Text.WordWrap
                text: measurePage.relaxText
                visible: !deviceHandler.measuring
                color: GameSettings.textColor
                fontSizeMode: Text.Fit
                minimumPixelSize: 10
                font.pixelSize: GameSettings.mediumFontSize
            }

            Text {
                id: text
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -parent.height*0.15
                font.pixelSize: parent.width * 0.45
                text: deviceHandler.hr
                visible: deviceHandler.measuring
                color: GameSettings.textColor
            }

            Item {
                id: minMaxContainer
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width*0.7
                height: parent.height * 0.15
                anchors.bottom: parent.bottom
                anchors.bottomMargin: parent.height*0.16
                visible: deviceHandler.measuring

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: deviceHandler.minHR
                    color: GameSettings.textColor
                    font.pixelSize: GameSettings.hugeFontSize

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.top
                        font.pixelSize: parent.font.pixelSize*0.8
                        color: parent.color
                        text: "MIN"
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: deviceHandler.maxHR
                    color: GameSettings.textColor
                    font.pixelSize: GameSettings.hugeFontSize

                    Text {
                        anchors.right: parent.right
                        anchors.bottom: parent.top
                        font.pixelSize: parent.font.pixelSize*0.8
                        color: parent.color
                        text: "MAX"
                    }
                }
            }

            Image {
                id: heart
                anchors.horizontalCenter: minMaxContainer.horizontalCenter
                anchors.verticalCenter: minMaxContainer.bottom
                width: parent.width * 0.2
                height: width
                source: "images/heart.png"
                smooth: true
                antialiasing: true

                SequentialAnimation{
                    id: heartAnim
                    running: deviceHandler.alive
                    loops: Animation.Infinite
                    alwaysRunToEnd: true
                    PropertyAnimation { target: heart; property: "scale"; to: 1.2; duration: 500; easing.type: Easing.InQuad }
                    PropertyAnimation { target: heart; property: "scale"; to: 1.0; duration: 500; easing.type: Easing.OutQuad }
                }
            }
        }

        Rectangle {
            id: timeSlider
            color: GameSettings.viewColor
            anchors.horizontalCenter: parent.horizontalCenter
            width: circle.width
            height: GameSettings.fieldHeight
            radius: GameSettings.buttonRadius

            Rectangle {
                height: parent.height
                radius: parent.radius
                color: GameSettings.sliderColor
                width: Math.min(1.0,__timeCounter / __maxTimeCount) * parent.width
            }

            Text {
                anchors.centerIn: parent
                color: "gray"
                text: (__maxTimeCount - __timeCounter).toFixed(0) + " s"
                font.pixelSize: GameSettings.bigFontSize
            }
        }
    }

    GameButton {
        id: startButton
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: GameSettings.fieldMargin
        width: circle.width
        height: GameSettings.fieldHeight
        enabled: !deviceHandler.measuring
        radius: GameSettings.buttonRadius

        onClicked: start()

        Text {
            anchors.centerIn: parent
            font.pixelSize: GameSettings.tinyFontSize
            text: qsTr("START")
            color: startButton.enabled ? GameSettings.textColor : GameSettings.disabledTextColor
        }
    }
}
