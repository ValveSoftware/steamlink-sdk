/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

import QtQuick 1.0

Rectangle {
    id: fileMenu
    height: 480; width:1000
    property color buttonBorderColor: "#7F8487"
    property color buttonFillColor: "#8FBDCACD"
    property string fileContent:directory.fileContent

    //the menuName is accessible from outside this QML file
    property string menuName: "File"

    //used to divide the screen into parts.
    property real partition: 1/3

    color: "#6C646A"
        gradient: Gradient {
                        GradientStop { position: 0.0; color: "#6C646A" }
                        GradientStop { position: 1.0; color: Qt.darker("#6A6D6A") }
        }

    Directory {
        id:directory
        filename: textInput.text
        onDirectoryChanged:fileDialog.notifyRefresh()
    }

    Rectangle {
        id:actionContainer

        //make this rectangle invisible
        color:"transparent"
        anchors.left: parent.left

        //the height is a good proportion that creates more space at the top of
        //the column of buttons
        width: fileMenu.width * partition; height: fileMenu.height

        Column {
            anchors.centerIn: parent
            spacing: parent.height/32
            Button {
                id: saveButton
                label: "Save"
                borderColor: buttonBorderColor
                buttonColor: buttonFillColor
                width: actionContainer.width/ 1.3
                height:actionContainer.height / 8
                labelSize:24
                onButtonClick: {
                    directory.fileContent = textArea.textContent
                    directory.filename = textInput.text
                    directory.saveFile()
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
            Button {
                id: loadButton
                width: actionContainer.width/ 1.3
                height:actionContainer.height/ 8
                buttonColor: buttonFillColor
                borderColor: buttonBorderColor
                label: "Load"
                labelSize:24
                onButtonClick:{
                    directory.filename = textInput.text
                    directory.loadFile()
                    textArea.textContent = directory.fileContent
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
            Button {
                id: newButton
                width: actionContainer.width/ 1.3
                height: actionContainer.height/ 8
                buttonColor: buttonFillColor
                borderColor: buttonBorderColor
                label: "New"
                labelSize: 24
                onButtonClick:{
                    textArea.textContent = ""
                    textInput.text = ""
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }

            }
            Rectangle {
                id: space
                width: actionContainer.width/ 1.3
                height: actionContainer.height / 16
                color: "transparent"
            }
            Button {
                id: exitButton
                width: actionContainer.width/ 1.3
                height: actionContainer.height/ 8
                label: "Exit"
                labelSize: 24
                buttonColor: buttonFillColor
                borderColor: buttonBorderColor
                onButtonClick: Qt.quit()
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
        }
    }
    Rectangle {
        id:dialogContainer

        width: 2*fileMenu.width * partition; height: fileMenu.height
        anchors.right:parent.right
        color: "transparent"

        Column {
            anchors.centerIn: parent
            spacing: parent.height /640
            FileDialog {
                id:fileDialog
                height: 2*dialogContainer.height * partition
                width: dialogContainer.width
                onSelectChanged: textInput.text = selectedFile
            }

            Rectangle {
                id:lowerPartition
                height: dialogContainer.height * partition; width: dialogContainer.width
                color: "transparent"

                Rectangle {
                    id: nameField
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#806F6F6F" }
                        GradientStop { position: 1.0; color: "#136F6F6F" }
                    }
                    radius: 10
                    anchors { centerIn:parent; leftMargin: 15; rightMargin: 15; topMargin: 15 }
                    height: parent.height-15
                    width: parent.width -20
                    border { color: "#4A4A4A"; width:1 }

                    TextInput {
                        id: textInput
                        z:2
                        anchors { bottom: parent.bottom; topMargin: 10; horizontalCenter: parent.horizontalCenter }
                        width: parent.width - 10
                        height: parent.height -10
                        font.pointSize: 40
                        color: "lightsteelblue"
                        focus: true
                    }
                    Text {
                        id: textInstruction
                        anchors.centerIn: parent
                        text: "Select file name and press save or load"
                        font {pointSize: 11; weight: Font.Light; italic: true}
                        color: "lightblue"
                        z: 2
                        opacity: (textInput.text == "") ? 1 : 0
                    }
                    Text {
                        id:fieldLabel
                        anchors { top: parent.top; left: parent.left }
                        text: "  file name: "
                        font { pointSize: 11; weight: Font.Light; italic: true }
                        color: "lightblue"
                        z:2
                    }
                    MouseArea {
                            anchors.centerIn:parent
                            width: nameField.width; height: nameField.height
                            onClicked: {
                                textInput.text = ""
                                textInput.focus = true
                                textInput.forceFocus()
                            }
                    }
                }
            }
        }
    }
}
