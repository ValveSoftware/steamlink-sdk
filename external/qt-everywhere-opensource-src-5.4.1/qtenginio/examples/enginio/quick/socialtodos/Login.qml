/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0 as Controls
import Enginio 1.0

Rectangle {
    width: 400
    height: 600

    Rectangle {
        id: header
        anchors.top: parent.top
        width: parent.width
        height: 70 * scaleFactor
        color: "white"

        Row {
            id: logo
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: -4
            spacing: 4
            Image {
                source: "qrc:images/enginio.png"
                width: 160 * scaleFactor ; height: 60 * scaleFactor
                fillMode: Image.PreserveAspectFit
            }
            Text {
                text: "Todos"
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -3
                font.bold: true
                font.pixelSize: 46 * scaleFactor
                color: "#555"
            }
        }
        Rectangle {
            width: parent.width ; height: 1
            anchors.bottom: parent.bottom
            color: "#bbb"
        }
    }

    BorderImage {
        id: input

        width: parent.width
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        source: "qrc:images/delegate.png"
        border.left: 5; border.top: 5
        border.right: 5; border.bottom: 5

        Rectangle {
            y: -1 ; height: 1
            width: parent.width
            color: "#bbb"
        }
        Rectangle {
            y: 0 ; height: 1
            width: parent.width
            color: "white"
        }
    }

    Column {
        anchors.centerIn: parent
        anchors.alignWhenCentered: true
        width: 360 * scaleFactor
        spacing: 14 * intScaleFactor

        TextField {
            id: nameInput
            onAccepted: passwordInput.forceActiveFocus()
            placeholderText: "Username"
            KeyNavigation.tab: passwordInput
        }

        TextField {
            id: passwordInput
            onAccepted: login()
            placeholderText: "Password"
            echoMode: TextInput.Password
            KeyNavigation.tab: loginButton
        }

        Row {
            // button
            spacing: 20 * scaleFactor
            width: 360 * scaleFactor
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.alignWhenCentered: true
            TouchButton {
                id: loginButton
                text: "Login"
                baseColor: "#7a5"
                width: (parent.width - parent.spacing)/2
                onClicked: login()
                enabled: enginioClient.authenticationState !== Enginio.Authenticating && nameInput.text.length && passwordInput.text.length
                KeyNavigation.tab: registerButton
            }
            TouchButton {
                id: registerButton
                text: "Register"
                onClicked: registerAndLogin()
                width: (parent.width - parent.spacing)/2
                enabled: enginioClient.authenticationState !== Enginio.Authenticating && nameInput.text.length && passwordInput.text.length
                KeyNavigation.tab: nameInput
            }
        }
    }

    Text {
        id: statusText
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 70 * scaleFactor
        font.pixelSize: 18 * scaleFactor
        color: "#444"
    }

    Component.onCompleted: {
        enginioClient.sessionAuthenticationError.connect(function(reply){
            statusText.text = "Login failed: " + reply.errorString
        })

       enginioClient.sessionAuthenticated.connect(function(reply){
           addUserToAllUsersGroup(reply.data.enginio_data.user.id)
        })
    }

    Controls.Stack.onStatusChanged: {
        if (Controls.Stack.status == Controls.Stack.Activating) {
            nameInput.text = ""
            passwordInput.text = ""
            statusText.text = ""
            nameInput.forceActiveFocus()
        }
    }

    function login() {
        statusText.text = "Logging in..."
        enginioClient.identity = null
        auth.user = nameInput.text
        auth.password = passwordInput.text
        enginioClient.identity = auth
    }

    // Register a new user and add her to the group "allUsers"
    function registerAndLogin() {
        statusText.text = "Creating user account..."
        var createAccount = enginioClient.create({ "username": nameInput.text, "password": passwordInput.text }, Enginio.UserOperation)
        createAccount.finished.connect(function() {
            if (createAccount.errorType !== EnginioReply.NoError) {
                statusText.text = "Account creation failed: " + createAccount.errorString
            } else {
                login()
            }
        })
    }


    function addUserToAllUsersGroup(userId) {
        //![queryUsergroup]
        var groupQuery = enginioClient.query({ "query": { "name" : "allUsers" } }, Enginio.UsergroupOperation)
        //![queryUsergroup]

        groupQuery.finished.connect(function(){
            if (groupQuery.errorType !== EnginioReply.NoError) {
                statusText.text = groupQuery.errorString
            } else if (groupQuery.data.results.length === 0 ){
                statusText.text = "Usergroup 'allUsers' not found, check required backend configuration from Social Todos example documentation."
            } else {
                var addUserToGroupData = {
                    "id": groupQuery.data.results[0].id,
                    "member" : {
                        "id": userId,
                        "objectType": "users"
                    }
                }
                var addUserToGroup = enginioClient.create(addUserToGroupData, Enginio.UsergroupMembersOperation)
                addUserToGroup.finished.connect(function(){
                    if (addUserToGroup.errorType === EnginioReply.NoError) {
                        switchToListsView()
                    } else if (addUserToGroup.errorType === EnginioReply.BackendError &&
                               addUserToGroup.backendStatus === 400) {
                        //User already present in group, OK to proceed
                        switchToListsView()
                    } else {
                        statusText.text = "User add to group failed: " + JSON.stringify(addUserToGroup.data.errors[0])
                    }
                })
            }
        })
    }

    function switchToListsView() {
        mainView.push({ item: lists, properties: {"username": enginioClient.identity.user}})
    }
}
