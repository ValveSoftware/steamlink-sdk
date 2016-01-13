/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
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

Rectangle {
    id: screen
    color: "#F0EBED"
    property string message: heartRate.message
    onMessageChanged: {
        if (heartRate.message != "Scanning for devices..." && heartRate.message != "Low Energy device found. Scanning for more...") {
            background.visible = false;
            demoMode.visible = true;
        }
        else {
            demoMode.visible = false;
            background.visible = true;
        }
    }

    Rectangle {
        id:select
        width: parent.width
        anchors.top: parent.top
        height: 80
        color: "#F0EBED"
        border.color: "#3870BA"
        border.width: 2
        radius: 10

        Text {
            id: selectText
            color: "#3870BA"
            font.pixelSize: 34
            anchors.centerIn: parent
            text: "Select Device"
        }
    }

    Rectangle {
        id: spinner
        width: parent.width
        anchors.top: select.bottom
        anchors.bottom: demoMode.top
        visible: false
        color: "#F0EBED"
        z: 100

        Rectangle {
            id: inside
            anchors.centerIn: parent
            Image {
                id: background

                width:100
                height:100
                anchors.horizontalCenter: parent.horizontalCenter

                source: "busy_dark.png"
                fillMode: Image.PreserveAspectFit
                NumberAnimation on rotation { duration: 3000; from:0; to: 360; loops: Animation.Infinite}
            }

            Text {
                id: infotext
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: background.bottom
                text: heartRate.message
                color: "#8F8F8F"
            }
        }
    }

    Component.onCompleted: {
        heartRate.deviceSearch();
        spinner.visible=true;
    }

    ListView {
        id: theListView
        width: parent.width
        onModelChanged: spinner.visible=false
        anchors.top: select.bottom
        anchors.bottom: demoMode.top
        model: heartRate.name

        delegate: Rectangle {
            id: box
            height:140
            width: parent.width
            color: "#3870BA"
            border.color: "#F0EBED"
            border.width: 5
            radius: 15

            MouseArea {
                anchors.fill: parent
                onPressed: { box.color= "#3265A7"; box.height=110}
                onClicked: {
                    heartRate.connectToService(modelData.deviceAddress);
                    pageLoader.source="monitor.qml";
                }
            }

            Text {
                id: device
                font.pixelSize: 30
                text: modelData.deviceName
                anchors.top: parent.top
                anchors.topMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#F0EBED"
            }

            Text {
                id: deviceAddress
                font.pixelSize: 30
                text: modelData.deviceAddress
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#F0EBED"
            }
        }
    }

    Button {
        id:demoMode
        buttonWidth: parent.width
        buttonHeight: 0.1*parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: scanAgain.top
        text: "Run Demo"
        onButtonClick: {
            heartRate.startDemo();
            pageLoader.source="monitor.qml";
        }
    }

    Button {
        id:scanAgain
        buttonWidth: parent.width
        buttonHeight: 0.1*parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        text: "Menu"
        onButtonClick: pageLoader.source="main.qml"
    }
}
