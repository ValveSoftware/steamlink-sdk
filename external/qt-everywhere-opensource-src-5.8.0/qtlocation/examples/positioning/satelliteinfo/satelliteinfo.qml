/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.0
import Local 1.0

Rectangle {
    id: page
    width: 360
    height: 360

    SatelliteModel {
        id: satelliteModel
        running: true
        onErrorFound: errorLabel.text = qsTr("Last Error: %1", "%1=error number").arg(code)
    }

    Item {
        id: header
        height: column.height + 30
        width: parent.width
        state: "running"

        anchors.top: parent.top

        function toggle()
        {
            switch (header.state) {
            case "single": header.state = "running"; break;
            default:
            case "running": header.state = "stopped"; break;
            case "stopped": header.state = "single"; break;
            }
        }

        function enterSingle()
        {
            satelliteModel.singleRequestMode = true;
            satelliteModel.running = true;
        }

        function enterRunning()
        {
            satelliteModel.running = false;
            satelliteModel.singleRequestMode = false;
            satelliteModel.running = true;
        }

        states: [
            State {
                name: "stopped"
                PropertyChanges { target: startStop; bText: qsTr("Single") }
                PropertyChanges {
                    target: modeLabel; text: qsTr("Current Mode: stopped")
                }
                PropertyChanges { target: satelliteModel; running: false; }
            },
            State {
                name: "single"
                PropertyChanges { target: startStop; bText: qsTr("Start") }
                PropertyChanges {
                    target: modeLabel; text: qsTr("Current Mode: single")
                }
                StateChangeScript { script: header.enterSingle(); }
            },
            State {
                name: "running"
                PropertyChanges { target: startStop; bText: qsTr("Stop") }
                PropertyChanges {
                    target: modeLabel; text: qsTr("Current Mode: running")
                }
                StateChangeScript { script: header.enterRunning(); }
            }
        ]

        Column {
            id: column
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.margins: 7
            Text {
                id:  overview
                text: satelliteModel.satelliteInfoAvailable
                      ? qsTr("Known Satellites")
                      : qsTr("Known Satellites (Demo Mode)")
                font.pointSize: 12
            }

            Text {
                id: modeLabel
                font.pointSize: 12
            }

            Text {
                id: errorLabel
                text: qsTr("Last Error: None")
                font.pointSize: 12
            }
        }
        Rectangle {
            id: startStop
            border.color: "black"
            border.width: 3
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 7
            radius: 10
            height: maxField.height*1.4
            width: maxField.width*1.4

            property string bText: qsTr("Stop");

            Text { //need this for sizing
                id: maxField
                text: qsTr("Single")
                font.pointSize: 13
                opacity: 0
            }

            Text {
                id: buttonText
                anchors.centerIn: parent
                text: startStop.bText
                font.pointSize: 13
            }

            MouseArea {
                anchors.fill: parent
                onPressed: { startStop.color = "lightGray" }
                onClicked: { header.toggle() }
                onReleased: { startStop.color = "white" }
            }
        }
    }

    Rectangle {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: rect.myMargin
        border.width: 3
        radius: 10
        border.color: "black"

        Item {
            id: rect
            anchors.fill: parent
            anchors.margins: myMargin
            property int myMargin: 7

            Row {
                id: view
                property int rows: repeater.model.entryCount
                property int singleWidth: ((rect.width - scale.width)/rows )-rect.myMargin
                spacing: rect.myMargin

                Rectangle {
                    id: scale
                    width: strengthLabel.width+10
                    height: rect.height
                    color: "#32cd32"
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: lawngreenRect.top
                        font.pointSize: 11
                        text: "50"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        font.pointSize: 11
                        text: "100"
                    }

                    Rectangle {
                        id: redRect
                        width: parent.width
                        color: "red"
                        height: parent.height*10/100
                        anchors.bottom: parent.bottom
                        Text {
                            id: strengthLabel
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            font.pointSize: 11
                            text: "00"
                        }
                    }
                    Rectangle {
                        id: orangeRect
                        height: parent.height*10/100
                        anchors.bottom: redRect.top
                        width: parent.width
                        color: "#ffa500"
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            font.pointSize: 11
                            text: "10"
                        }
                    }
                    Rectangle {
                        id: goldRect
                        height: parent.height*10/100
                        anchors.bottom: orangeRect.top
                        width: parent.width
                        color: "#ffd700"
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            font.pointSize: 11
                            text: "20"
                        }
                    }
                    Rectangle {
                        id: yellowRect
                        height: parent.height*10/100
                        anchors.bottom: goldRect.top
                        width: parent.width
                        color: "yellow"
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            font.pointSize: 11
                            text: "30"
                        }
                    }
                    Rectangle {
                        id: lawngreenRect
                        height: parent.height*10/100
                        anchors.bottom: yellowRect.top
                        width: parent.width
                        color: "#7cFc00"
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            font.pointSize: 11
                            text: "40"
                        }
                    }
                }

                Repeater {
                    id: repeater
                    model: satelliteModel
                    delegate: Rectangle {
                        height: rect.height
                        width: view.singleWidth
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: parent.height*signalStrength/100
                            color: isInUse ? "#7FFF0000" : "#7F0000FF"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            text: satelliteIdentifier
                        }
                    }
                }
            }
        }
    }
}

