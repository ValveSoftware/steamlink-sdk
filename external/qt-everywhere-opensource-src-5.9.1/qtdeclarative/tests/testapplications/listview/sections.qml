/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

Rectangle {
    height: 400
    width: 300
    property int sets: 1

    ListModel {
        id: listmodel
        Component.onCompleted: addNames()
    }

    ListView {
        id: listview
        model: listmodel
        height: 300
        width: 200
        clip: true
        anchors.centerIn: parent

        section.delegate: del1
        section.criteria: ViewSection.FirstCharacter
        section.property: "name"
        delegate: Rectangle {
            height: 50
            width: 200
            color: "gold"
            border.color: "black"
            Text {
                anchors.centerIn: parent
                text: model.name+" ["+model.id+"]"
                color: "black"
                font.bold: true
            }
        }
    }

    function addNames() {
        var names = ["Alpha","Bravo","Charlie","Delta","Echo","Foxtrot",
                     "Golf","Hotel","India","Juliet","Kilo","Lima","Mike",
                     "November","Oscar","Papa","Quebec","Romeo","Sierra","Tango",
                     "Uniform","Victor","Whiskey","XRay","Yankee","Zulu"];
        for (var i=0;i<names.length;++i)
            listmodel.insert(sets*i, {"name":names[i], "id": "id"+i});
        sets++;
    }

    Component {
        id: del1
        Rectangle {
            height: 50
            width: 200
            color: "white"
            border.color: "black"
            border.width: 3
            Text {
                anchors.centerIn: parent
                text: section
            }
        }
    }

    Component {
        id: del2
        Rectangle {
            height: 50
            width: 200
            color: "lightsteelblue"
            border.color: "orange"
            Text {
                anchors.centerIn: parent
                text: section
            }
        }
    }

    Rectangle {
        anchors.fill: listview
        color: "transparent"
        border.color: "green"
        border.width: 3
    }

    Row {
        spacing: 3
        Rectangle {
            height: 40
            width: 70
            color: "blue"
            Text {
                color: "white"
                anchors.centerIn: parent
                text: "Criteria"
            }
            radius: 5
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listview.section.criteria = listview.section.criteria == ViewSection.FirstCharacter ?
                           ViewSection.FullString : ViewSection.FirstCharacter
                }
            }
        }
        Rectangle {
            height: 40
            width: 70
            color: "blue"
            Text {
                color: "white"
                anchors.centerIn: parent
                text: "Property"
            }
            radius: 5
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listview.section.property = listview.section.property == "name" ? "id" : "name";
                    console.log(listview.section.property)
                }
            }
        }
        Rectangle {
            height: 40
            width: 75
            color: "blue"
            Text {
                color: "white"
                anchors.centerIn: parent
                text: "Delegate"
            }
            radius: 5
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log("Change delegate")
                    listview.section.delegate = listview.section.delegate == del1 ? del2 : del1
                }
            }
        }
        Rectangle {
            height: 40
            width: 40
            color: "blue"
            Text {
                color: "white"
                anchors.centerIn: parent
                text: "+"
                font.bold: true
            }
            radius: 5
            MouseArea {
                anchors.fill: parent
                onClicked: { addNames(); }
            }
        }
    }
}
