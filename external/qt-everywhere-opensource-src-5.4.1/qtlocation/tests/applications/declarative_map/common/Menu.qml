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

Item {
    id: menu
    property int gap: 0
    property int button: 0
    property alias orientation: menuView.orientation
    property alias count: menuModel.count
    property int itemHeight  //to create menu just set menu item height and width, do not set menu's height and width explicitly
    property int itemWidth
//    height: (menuView.orientation == ListView.Horizontal)? itemHeight : itemHeight * count
    width: (menuView.orientation == ListView.Horizontal)? itemWidth * count : itemWidth

    signal clicked

    function setModel(objects)
    {
        for (var i=0; i< objects.length; i++){
            menuModel.append({"label": objects[i], "enabledItem" : true})
        }
        height = (menuView.orientation == ListView.Horizontal)? itemHeight : itemHeight * count
    }

    function disableItem(index){
        menuModel.set(index, {"enabledItem": false})
    }

    function enableItem(index){
        menuModel.set(index, {"enabledItem": true})
    }

    ListModel {
        id: menuModel
    }

    Component{
        id: menuItemDelegate
        Item {
            height: itemHeight
            width: itemWidth

            BorderImage {
                id: menuItemImage;
                source: "../resources/menuItem.sci";
                height: parent.height + 14;
                width: parent.width
                y: -7
            }

            Text {
                id: menuItemText
                text: label;
                elide: Text.ElideLeft
                font.bold: true;
                color: "white"
                style: Text.Raised;
                styleColor: "dimgrey"
                anchors.verticalCenter: parent.verticalCenter
                Component.onCompleted: {
                    if (menuView.orientation == ListView.Horizontal){
                        anchors.horizontalCenter = parent.horizontalCenter
                    }
                    else {
                        anchors.left = parent.left
                        anchors.leftMargin = 10
                    }
                }
            }
            MouseArea {
                id: mouseRegion
                anchors.fill: parent
                hoverEnabled: true
                enabled: enabledItem
                onClicked: {
                    button = index
                    menu.clicked()
                }
            }

            states: [
                State {
                    name: "Pressed"
                    when: mouseRegion.pressed == true
                    PropertyChanges { target: menuItemImage; source: "../resources/menuItem_pressed.png"}
                    PropertyChanges { target: menuItemText; style: Text.Sunken }
                },
                State {
                    name: "Hovered"
                    when: mouseRegion.containsMouse == true
                    PropertyChanges { target: menuItemImage; source: "../resources/menuItem_hovered.png"}
                },
                State {
                    name: "Disabled"
                    when: mouseRegion.enabled == false
                    PropertyChanges { target: menuItemText; color: "grey"}
                }
            ]
        }
    }

    ListView {
        id: menuView
        anchors.fill: parent
        model: menuModel
        delegate: menuItemDelegate
        spacing: gap
        interactive: false
    }
}
