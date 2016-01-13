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
    id: menuBar
    width: 1000; height:300
    color:"transparent"
    property color fileColor: "plum"
    property color editColor: "powderblue"

    property real partition: 1/10

    Column {
        anchors.fill: parent
        //container for the header and the buttons
        z: 1
        Rectangle {
            id: labelList
            height:menuBar.height*partition
            width: menuBar.width
            color: "beige"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#8C8F8C" }
                GradientStop { position: 0.17; color: "#6A6D6A" }
                GradientStop { position: 0.98;color: "#3F3F3F" }
                GradientStop { position: 1.0; color: "#0e1B20" }
            }
            Text {
                height: parent.height
                anchors { right: labelRow.left ; verticalCenter: parent.bottom }
                text: "menu:    "
                color: "lightblue"
                font { weight: Font.Light; italic: true }
                smooth: true
            }
            //row displays its children in a vertical row
            Row {
                id: labelRow
                anchors.centerIn: parent
                spacing:40
                Button {
                    id: fileButton
                    height: 20; width: 50
                    label: "File"
                    buttonColor : menuListView.currentIndex == 0? fileColor : Qt.darker(fileColor, 1.5)
                    scale: menuListView.currentIndex == 0? 1.25: 1
                    labelSize: menuListView.currentIndex == 0? 16:12
                    radius: 1
                    smooth:true
                    //on a button click, change the list's currently selected item to FileMenu
                    onButtonClick: menuListView.currentIndex = 0
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: fileColor }
                        GradientStop { position: 1.0; color: "#136F6F6F" }
                    }
                }
                Button {
                    id: editButton
                    height: 20; width: 50
                    buttonColor : menuListView.currentIndex == 1? Qt.darker(editColor, 1.5) : Qt.darker(editColor, 1.9)
                    scale: menuListView.currentIndex == 1? 1.25: 1
                    label: "Edit"
                    radius: 1
                    labelSize: menuListView.currentIndex == 1? 16:12
                    smooth:true
                    //on a button click, change the list's currently selected item to EditMenu
                    onButtonClick: menuListView.currentIndex = 1
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: editColor }
                        GradientStop { position: 1.0; color: "#136F6F6F" }
                    }
                }
            }
        }

        //list view will display a model according to a delegate
        ListView {
            id: menuListView
            width:menuBar.width; height: 9*menuBar.height*partition

            //the model contains the data
            model: menuListModel

            //control the movement of the menu switching
            snapMode: ListView.SnapOneItem
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 5000
            highlightFollowsCurrentItem: true
            highlightMoveDuration:240
            highlightRangeMode: ListView.StrictlyEnforceRange
        }
    }
    //a list of visual items already have delegates handling their display
    VisualItemModel {
        id: menuListModel

        FileMenu {
            id:fileMenu
            width: menuListView.width; height: menuListView.height
            color: fileColor
        }
        EditMenu {
            color: editColor
            width:  menuListView.width; height: menuListView.height
        }
    }
}
