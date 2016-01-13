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
    id: editMenu
    height: 480; width:1000
    color: "powderblue"
    property color buttonBorderColor: "#7A8182"
    property color buttonFillColor: "#61BDCACD"
    property string menuName:"Edit"
    gradient: Gradient {
        GradientStop { position: 0.0; color: "#6A7570" }
        GradientStop { position: 1.0; color: Qt.darker("#6A7570") }
    }

    Rectangle {
        id:actionContainer
         color:"transparent"
        anchors.centerIn: parent
        width: parent.width; height: parent.height / 5
        Row {
            anchors.centerIn: parent
            spacing: parent.width/9
            Button {
                id: loadButton
                buttonColor: buttonFillColor
                label: "Copy"
                labelSize: 16
                borderColor: buttonBorderColor
                height: actionContainer.height; width: actionContainer.width/6
                onButtonClick: textArea.copy()
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
            Button {
                id: saveButton
                height: actionContainer.height; width: actionContainer.width/6
                buttonColor: buttonFillColor
                label: "Paste"
                borderColor: buttonBorderColor
                labelSize: 16
                onButtonClick: textArea.paste()
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
            Button {
                id: exitButton
                label: "Select All"
                height: actionContainer.height; width: actionContainer.width/6
                labelSize: 16
                buttonColor: buttonFillColor
                borderColor:buttonBorderColor
                onButtonClick: textArea.selectAll()
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(buttonFillColor,1.25) }
                    GradientStop { position: 0.67; color: Qt.darker(buttonFillColor,1.3) }
                }
            }
        }
    }
}
