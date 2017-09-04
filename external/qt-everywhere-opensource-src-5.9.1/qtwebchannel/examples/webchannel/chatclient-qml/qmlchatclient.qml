/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 basysKom GmbH, author Bernd Lamecker <bernd.lamecker@basyskom.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
import QtQuick.Window 2.0
import QtQuick.Layouts 1.1
import Qt.WebSockets 1.0
import "qwebchannel.js" as WebChannel

ApplicationWindow {
    id: root
    title: qsTr("Hello World")
    width: 640
    height: 480

    property var channel;

    WebSocket {
        id: socket

        url: "ws://localhost:12345";
        active: false

        // the following three properties/functions are required to align the QML WebSocket API with the HTML5 WebSocket API.
        property var send: function (arg) {
            sendTextMessage(arg);
        }

        onTextMessageReceived: {
            onmessage({data: message});
        }
        property var onmessage;

        onStatusChanged: if (socket.status == WebSocket.Error) {
                             console.error("Error: " + socket.errorString)
                         } else if (socket.status == WebSocket.Closed) {
                             messageBox.text += "\nSocket closed"
                         } else if (socket.status == WebSocket.Open) {
                             //open the webchannel with the socket as transport
                             new WebChannel.QWebChannel(socket, function(ch) {
                                 root.channel = ch;

                                 //connect to the changed signal of the userList property
                                 ch.objects.chatserver.userListChanged.connect(function(args) {
                                     userlist.text = '';
                                     ch.objects.chatserver.userList.forEach(function(user) {
                                         userlist.text += user + '\n';
                                     });
                                 });
                                 //connect to the newMessage signal
                                 ch.objects.chatserver.newMessage.connect(function(time, user, message) {
                                     chat.text = chat.text + "[" + time + "] " + user + ": " +  message + '\n';
                                 });
                                 //connect to the keep alive signal
                                 ch.objects.chatserver.keepAlive.connect(function(args) {
                                     if (loginName.text !== '')
                                         //and call the keep alive response method as an answer
                                         ch.objects.chatserver.keepAliveResponse(loginName.text)
                                });
                            });
                         }
    }

    GridLayout {
        id: grid
        columns: 2
        anchors.fill: parent
        Text {
            id: chat
            text: ""
            Layout.fillHeight: true
            Layout.fillWidth: true
        }

        Text {
            id: userlist
            text: ""
            width: 150
            Layout.fillHeight: true
        }
        TextField {
            id: message
            height: 50
            Layout.columnSpan: 2
            Layout.fillWidth: true

            onEditingFinished: {
                if (message.text.length)
                    //call the sendMessage method to send the message
                    root.channel.objects.chatserver.sendMessage(loginName.text, message.text);
                message.text = '';
            }
        }
    }


    Window {
       id: loginWindow;
       title: "Login";
       modality: Qt.ApplicationModal

       TextField {
           id: loginName

           anchors.top: parent.top
           anchors.horizontalCenter: parent.horizontalCenter
       }
       Button {
           anchors.top: loginName.bottom
           anchors.horizontalCenter: parent.horizontalCenter
           id: loginButton
           text: "Login"

           onClicked: {
               //call the login method
               root.channel.objects.chatserver.login(loginName.text, function(arg) {
                   //check the return value for success
                   if (arg === true) {
                       loginError.visible = false;
                       loginWindow.close();
                   } else {
                       loginError.visible = true;
                   }
               });

           }
       }
       Text {
           id: loginError
           anchors.top: loginButton.bottom
           anchors.horizontalCenter: parent.horizontalCenter
           text: "Name already in use"
           visible: false;
       }
    }

    Component.onCompleted: {
        loginWindow.show();
        socket.active = true; //connect
    }
}
