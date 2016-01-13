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
import "common" as Common

Item {
    id: dialog
    signal goButtonClicked
    signal cancelButtonClicked

    anchors.fill: parent

    property alias title: titleBar.text
    property alias dialogModel: dialogModel
    property alias length: dialogModel.count
    property int gap: 20
    property int listItemHeight: 54

    function setModel(objects)
    {
        for (var i=0; i< objects.length; i++){
            dialogModel.append({"label": objects[i][0], "inputText": objects[i][1]})
        }
    }

    Rectangle {
        id: fader
        anchors.fill: parent
        opacity: 0.7
        color:  "darkgrey"
        MouseArea {
            id: mouseArea
            anchors.fill: parent
        }
    }

    Rectangle {
        id: dialogRectangle

        color: "lightsteelblue"
        opacity: 1
        width: parent.width - gap;
        height: listview.height + titleBar.height + buttonGo.height + gap*2

        anchors {
            top: parent.top
            topMargin: 50
            left: parent.left
            leftMargin: gap/2
        }

        border.width: 1
        border.color: "darkblue"
        radius: 5

        Common.TitleBar {
            id: titleBar;
            width: parent.width; height: 40;
            anchors.top: parent.top; anchors.left: parent.left;
            opacity: 0.9; text: dialog.title;
            onClicked: { dialog.cancelButtonClicked() }
        }

        ListModel {
            id: dialogModel
        }

        Component{
            id: listDelegate
            Column {
                id: column1
                height: listItemHeight
                Text { id: fieldTitle; text: label; height: 24;}
                Rectangle {
                    id: inputRectangle
                    width: dialogRectangle.width - gap; height: 30
                    color: "whitesmoke"
                    border.width: 1
                    radius: 5
                    TextInput {
                        id: inputField
                        text: inputText
                        focus: true
                        width: parent.width - anchors.leftMargin

                        anchors {
                            left: parent.left;
                            verticalCenter: parent.verticalCenter;
                            leftMargin: 5
                        }
                        onTextChanged:
                        {
                            dialogModel.set(index, {"inputText": text})
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: inputField.forceActiveFocus();
                    }
                }
            }
        }

        ListView {
            id: listview
            anchors {
                top: titleBar.bottom
                topMargin: gap
                left: parent.left
                leftMargin: gap/2
            }
            model: dialogModel
            delegate: listDelegate
            spacing: gap
            interactive: false
            Component.onCompleted: {
                height = (listItemHeight + gap)*length
            }
        }

        Common.Button {
            id: buttonGo
            text: "Go!"
            anchors.top: listview.bottom
            width: 80; height: 32
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                dialog.goButtonClicked ()
            }
        }
    }
}

