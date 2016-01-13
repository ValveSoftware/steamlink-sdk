/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
//![imports]
import Enginio 1.0
//![imports]

Item {
    width: 400
    height: 640
    BackendHelper{
        id: backendHelper
    }

    Rectangle {
        id: root
        color: "#f4f4f4"
        anchors.fill: parent
        opacity: 1 - backendHelper.opacity

        //![model]
        EnginioModel {
            id: enginioModel
            client: EnginioClient {
                backendId: backendHelper.backendId
                onError: console.log("Enginio error:", JSON.stringify(reply.data))
            }
            query: {"objectType": "objects.todos" }
        }
        //![model]

        // A simple layout:
        // a listview and a line edit with button to add to the list
        Rectangle {
            id: header
            anchors.top: parent.top
            width: parent.width
            height: 70
            color: "white"

            Row {
                id: logo
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: -4
                spacing: 4
                Image {
                    source: "qrc:images/enginio.png"
                    width: 160 ; height: 60
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: "Todos"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -3
                    font.bold: true
                    font.pixelSize: 46
                    color: "#555"
                }
            }
            Rectangle {
                width: parent.width ; height: 1
                anchors.bottom: parent.bottom
                color: "#bbb"
            }
        }

        //![view]
        ListView {
            id: listview
            model: enginioModel
            delegate: listItemDelegate
            anchors.top: header.bottom
            anchors.bottom: footer.top
            width: parent.width
            clip: true

            // Animations
            add: Transition { NumberAnimation { properties: "y"; from: root.height; duration: 250 } }
            removeDisplaced: Transition { NumberAnimation { properties: "y"; duration: 150 } }
            remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 150 } }
        }
        //![view]

        BorderImage {
            id: footer

            width: parent.width
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

            //![append]

            BorderImage {

                anchors.left: parent.left
                anchors.right: addButton.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                source:"images/textfield.png"
                border.left: 14 ; border.right: 14 ; border.top: 8 ; border.bottom: 8

                TextInput{
                    id: textInput
                    anchors.fill: parent
                    clip: true
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 22
                    Text {
                        id: placeholderText
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: !(parent.text.length || parent.inputMethodComposing)
                        font: parent.font
                        text: "New todo..."
                        color: "#aaa"
                    }
                    onAccepted: {
                        enginioModel.append({"title": textInput.text, "completed": false})
                        textInput.text = ""
                    }
                }
            }

            Item {
                id: addButton

                width: 40 ; height: 40
                anchors.margins: 20
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                enabled: textInput.text.length
                Image {
                    source: addMouseArea.pressed ? "qrc:icons/add_icon_pressed.png" : "qrc:icons/add_icon.png"
                    anchors.centerIn: parent
                    opacity: enabled ? 1 : 0.5
                }
                MouseArea {
                    id: addMouseArea
                    anchors.fill: parent
                    onClicked: textInput.accepted()
                }
            }
        }
        //![append]

        Component {
            id: listItemDelegate

            BorderImage {
                id: item

                width: parent.width ; height: 70
                source: mouse.pressed ? "qrc:images/delegate_pressed.png" : "qrc:images/delegate.png"
                border.left: 5; border.top: 5
                border.right: 5; border.bottom: 5

                Image {
                    id: shadow
                    anchors.top: parent.bottom
                    width: parent.width
                    visible: !mouse.pressed
                    source: "qrc:images/shadow.png"
                }

                //![setProperty]
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (index !== -1 && _synced) {
                            enginioModel.setProperty(index, "completed", !completed)
                        }
                    }
                }
                //![setProperty]
                Image {
                    id: checkbox
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    width: 32
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                    source: completed ? "qrc:images/checkmark.png" : ""
                }

                //![delegate-properties]
                Text {
                    id: todoText
                    text: title
                    font.pixelSize: 26
                    color: "#333"

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: checkbox.right
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 40
                    elide: Text.ElideRight
                }
                //![delegate-properties]

                // Show a delete button when the mouse is over the delegate
                //![sync]
                Image {
                    id: removeIcon

                    source: removeMouseArea.pressed ? "qrc:icons/delete_icon_pressed.png" : "qrc:icons/delete_icon.png"
                    anchors.margins: 20
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    opacity: enabled ? 1 : 0.5
                    Behavior on opacity {NumberAnimation{duration: 100}}
                    //![remove]
                    MouseArea {
                        id: removeMouseArea
                        anchors.fill: parent
                        onClicked: enginioModel.remove(index)
                    }
                    //![remove]
                }
                //![sync]
            }
        }
    }
}
