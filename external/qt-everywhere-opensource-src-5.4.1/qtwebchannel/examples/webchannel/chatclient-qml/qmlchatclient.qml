/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2014 basysKom GmbH, author Bernd Lamecker <bernd.lamecker@basyskom.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
