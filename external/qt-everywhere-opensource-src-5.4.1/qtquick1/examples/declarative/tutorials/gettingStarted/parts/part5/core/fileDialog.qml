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
    id:dialog
    height: 200 * partition; width: 200
    color: "transparent"

    signal selectChanged()
    signal notifyRefresh()
    onNotifyRefresh:dirView.model = directory.files

    property string selectedFile
    property int selectedIndex: 0

    Rectangle {
        id: dirBox
        radius: 10
        anchors.centerIn:parent
        height: parent.height -15; width: parent.width -30

        Rectangle {
            id:header
            height: parent.height*0.1; width: parent.width
            radius:3
            z:1
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#8C8F8C" }
                GradientStop { position: 0.17; color: "#6A6D6A" }
                GradientStop { position: 0.98;color: "#3F3F3F" }
                GradientStop { position: 1.0; color: "#0e1B20" }
            }
            Text {
                height: header.height; anchors.centerIn: header
                text: "files:"
                color: "lightblue"
                font.weight: Font.Light
                font.italic: true
            }
        }
        GridView {
            id:dirView
            width:parent.width; height:parent.height*.9
            anchors.top: header.bottom
            cellWidth: 100; cellHeight: 75
            model: directory.files
            delegate: dirDelegate
            clip: true
            highlightMoveDuration: 40
        }
        Component {
            id: dirDelegate
            Rectangle {
                id:file
                color: "transparent"
                width: file.GridView.view.cellWidth; height: file.GridView.view.cellHeight

                Text {
                    id:fileName
                    width: parent.width
                    anchors.centerIn: parent
                    text: name
                    color: "#BDCACD"
                    font.weight: file.GridView.view.currentIndex == index ?  Font.DemiBold : Font.Normal
                    font.pointSize: file.GridView.view.currentIndex == index ?  12 : 10
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                }
                Rectangle {
                    id: selection
                    width: parent.width; height: parent.height
                    anchors.centerIn: parent
                    radius: 10
                    smooth: true
                    scale: file.GridView.view.currentIndex == index ?  1 : 0.5
                    opacity: file.GridView.view.currentIndex == index ?  1 : 0
                    Text {
                        id: overlay
                        width: parent.width
                        anchors.centerIn: parent
                        text: name
                        color: "#696167"
                        font.weight: Font.DemiBold
                        font.pointSize: 12
                        smooth: true
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Behavior on opacity { NumberAnimation{ duration: 45 } }
                    Behavior on scale { NumberAnimation{ duration: 45 } }
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.lighter("lightsteelblue",1.25) }
                        GradientStop { position: 0.67; color: Qt.darker("lightsteelblue",1.3) }
                    }
                    border.color: "lightsteelblue"
                    border.width: 1
                }
                MouseArea {
                    id:fileMouseArea
                    anchors.fill:parent
                    hoverEnabled: true

                    onClicked: {
                        file.GridView.view.currentIndex = index
                        selectedFile = directory.files[index].name
                        selectChanged()
                    }
                    onEntered: {
                            fileName.color = "lightsteelblue"
                            fileName.font.weight = Font.DemiBold
                        }
                    onExited: {
                            fileName.font.weight = Font.Normal
                            fileName.color = "#BDCACD"
                        }
                }
            }
        }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#A5333333" }
            GradientStop { position: 1.0; color: "#03333333" }
        }
    }
}
