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

import QtQuick 2.0
import Enginio 1.0

Rectangle {
    id: root
    width: 400
    height: 640
    color: "#f4f4f4"

    property string listId
    property string listName
    property string listCreator

    EnginioModel {
        id: enginioModel
        client: enginioClient
        query: {"objectType": "objects.todos",
                "query": { "todoList": { "id": listId, "objectType": "objects.todoLists" } }
        }
    }

    Header {
        id: header
        text: listName
        rightMargin: icon.width

        Item {
            id: icon
            height: parent.height
            width: parent.height
            anchors.right: parent.right
            Image {
                id: shareButton
                anchors.centerIn: parent
                source: mouse.pressed ? "qrc:icons/share_icon_pressed.png" : "qrc:icons/share_icon.png"
                width:  implicitWidth * scaleFactor
                height: implicitHeight * scaleFactor
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    onClicked: mainView.push({ item: shareItem, properties: { listId: listId, listName: listName }} )
                }
            }
        }
    }

    Component {
        id: shareItem
        ShareDialog {}
    }

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

    BorderImage {
        id: footer

        height: 70 * scaleFactor
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

        TextField {
            id: textInput
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: addButton.left
            anchors.margins: 16 * scaleFactor
            placeholderText: "New todo..."
            onAccepted: {
                enginioModel.append({"title": textInput.text, "done": false, "todoList": { "id": listId, "objectType": "objects.todoLists" } } )
                textInput.text = ""
            }
        }

        Item {
            id: addButton

            width: 40 * scaleFactor
            height: 40 * scaleFactor
            anchors.margins: 20 * scaleFactor
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            enabled: textInput.text.length
            Image {
                width:  implicitWidth * scaleFactor
                height: implicitHeight * scaleFactor
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

    Component {
        id: listItemDelegate

        BorderImage {
            id: item

            width: parent.width ; height: 70 * scaleFactor
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

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (index !== -1 && _synced) {
                        enginioModel.setProperty(index, "done", !done)
                    }
                }
            }
            Image {
                id: checkbox
                anchors.left: parent.left
                anchors.leftMargin: 16 * scaleFactor
                height: width
                width: 32 * scaleFactor
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
                source: done ? "qrc:images/checkmark.png" : ""
            }

            Text {
                id: todoText
                text: title
                font.pixelSize: 26 * scaleFactor
                color: "#333"

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: checkbox.right
                anchors.right: parent.right
                anchors.leftMargin: 16 * scaleFactor
                anchors.rightMargin: 40 * scaleFactor
                elide: Text.ElideRight
            }

            // Show a delete button when the mouse is over the delegate
            Image {
                id: removeIcon

                source: removeMouseArea.pressed ? "qrc:icons/delete_icon_pressed.png" : "qrc:icons/delete_icon.png"
                width:  implicitWidth * scaleFactor
                height: implicitHeight * scaleFactor
                anchors.margins: 20 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                opacity: enabled ? 1 : 0.5
                Behavior on opacity {NumberAnimation{duration: 100}}
                MouseArea {
                    id: removeMouseArea
                    anchors.fill: parent
                    onClicked: enginioModel.remove(index)
                }
            }
        }
    }
}
