/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtBluetooth module.
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
import QtBluetooth 5.3

Item {
    id: top

    Component.onCompleted: state = "begin"

    property string remoteDeviceName: ""
    property bool serviceFound: false

    //! [BtDiscoveryModel-1]
    BluetoothDiscoveryModel {
        id: btModel
        running: true
        discoveryMode: BluetoothDiscoveryModel.MinimalServiceDiscovery
    //! [BtDiscoveryModel-1]
        onRunningChanged : {
            if (!btModel.running && top.state == "begin" && !serviceFound) {
                searchBox.animationRunning = false;
                searchBox.appendText("\nNo service found. \n\nPlease start server\nand restart app.")
            }
        }

        onErrorChanged: {
            if (error != BluetoothDiscoveryModel.NoError && !btModel.running) {
                searchBox.animationRunning = false
                searchBox.appendText("\n\nDiscovery failed.\nPlease ensure Bluetooth is available.")
            }
        }

    //! [BtDiscoveryModel-2]
        onServiceDiscovered: {
            if (serviceFound)
                return
            serviceFound = true
            console.log("Found new service " + service.deviceAddress + " " + service.deviceName + " " + service.serviceName);
            searchBox.appendText("\nConnecting to server...")
            remoteDeviceName = service.deviceName
            socket.setService(service)
        }
    //! [BtDiscoveryModel-2]
    //! [BtDiscoveryModel-3]
        uuidFilter: "e8e10f95-1a70-4b27-9ccf-02010264e9c8"
    }
    //! [BtDiscoveryModel-3]

    //! [BluetoothSocket-1]
    BluetoothSocket {
        id: socket
        connected: true

        onSocketStateChanged: {
            console.log("Connected to server")
            top.state = "chatActive"
        }
    //! [BluetoothSocket-1]
    //! [BluetoothSocket-3]
        onStringDataChanged: {
            console.log("Received data: " )
            var data = remoteDeviceName + ": " + socket.stringData;
            data = data.substring(0, data.indexOf('\n'))
            chatContent.append({content: data})
    //! [BluetoothSocket-3]
            console.log(data);
    //! [BluetoothSocket-4]
        }
    //! [BluetoothSocket-4]
    //! [BluetoothSocket-2]
    }
    //! [BluetoothSocket-2]

    ListModel {
        id: chatContent
        ListElement {
            content: "Connected to chat server"
        }
    }


    Rectangle {
        id: background
        z: 0
        anchors.fill: parent
        color: "#5d5b59"
    }

    Search {
        id: searchBox
        anchors.centerIn: top
        opacity: 1
    }

    Rectangle {
        id: chatBox
        opacity: 0
        anchors.centerIn: top

        color: "#5d5b59"
        border.color: "black"
        border.width: 1
        radius: 5
        anchors.fill: parent

        function sendMessage()
        {
            // toogle focus to force end of input method composer
            var hasFocus = input.focus;
            input.focus = false;

            var data = input.text
            input.clear()
            chatContent.append({content: "Me: " + data})
            //! [BluetoothSocket-5]
            socket.stringData = data
            //! [BluetoothSocket-5]
            chatView.positionViewAtEnd()

            input.focus = hasFocus;
        }

        Item {
            anchors.fill: parent
            anchors.margins: 10

            InputBox {
                id: input
                Keys.onReturnPressed: chatBox.sendMessage()
                height: sendButton.height
                width: parent.width - sendButton.width - 15
                anchors.left: parent.left
            }

            Button {
                id: sendButton
                anchors.right: parent.right
                label: "Send"
                onButtonClick: chatBox.sendMessage()
            }


            Rectangle {
                height: parent.height - input.height - 15
                width: parent.width;
                color: "#d7d6d5"
                anchors.bottom: parent.bottom
                border.color: "black"
                border.width: 1
                radius: 5

                ListView {
                    id: chatView
                    width: parent.width-5
                    height: parent.height-5
                    anchors.centerIn: parent
                    model: chatContent
                    clip: true
                    delegate: Component {
                        Text {
                            font.pointSize: 14
                            text: modelData
                        }
                    }
                }
            }
        }
    }

    states: [
        State {
            name: "begin"
            PropertyChanges { target: searchBox; opacity: 1 }
            PropertyChanges { target: chatBox; opacity: 0 }
        },
        State {
            name: "chatActive"
            PropertyChanges { target: searchBox; opacity: 0 }
            PropertyChanges { target: chatBox; opacity: 1 }
        }
    ]
}
