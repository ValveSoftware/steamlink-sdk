/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
import Enginio 1.0
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0

ColumnLayout {
    anchors.margins: 3
    spacing: 3

    TextField {
        id: login
        Layout.fillWidth: true
        placeholderText: "Username"
    }

    TextField {
        id: password
        Layout.fillWidth: true
        placeholderText: "Password"
        echoMode: TextInput.PasswordEchoOnEdit
    }

    TextField {
        id: userFirstName
        Layout.fillWidth: true
        placeholderText: "First name"
    }

    TextField {
        id: userLastName
        Layout.fillWidth: true
        placeholderText: "Last name"
    }

    TextField {
        id: userEmail
        Layout.fillWidth: true
        placeholderText: "Email"
    }

    Button {
        id: proccessButton
        Layout.fillWidth: true
        enabled: login.text.length && password.text.length
        text: "Register"

        states: [
            State {
                name: "Registering"
                PropertyChanges {
                    target: proccessButton
                    text: "Registering..."
                    enabled: false
                }
            }
        ]

        onClicked: {
            proccessButton.state = "Registering"
            //![create]
            var reply = enginioClient.create(
                        { "username": login.text,
                          "password": password.text,
                          "email": userEmail.text,
                          "firstName": userFirstName.text,
                          "lastName": userLastName.text
                        }, Enginio.UserOperation)
            //![create]
            reply.finished.connect(function() {
                    proccessButton.state = ""
                    if (reply.errorType !== EnginioReply.NoError) {
                        log.text = "Failed to create an account:\n" + JSON.stringify(reply.data, undefined, 2) + "\n\n"
                    } else {
                        log.text = "Account Created.\n"
                    }
                })
        }
    }

    TextArea {
        id: log
        readOnly: true
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
