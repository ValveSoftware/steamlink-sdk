/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

Rectangle {
    width: 300
    height: 400

    ListView {
        id: listview
        model: model1
        delegate: delegate1
        anchors.fill: parent
        anchors.margins: 20
    }

    Component {
        id: delegate1
        Rectangle {
            height: listview.orientation == ListView.Horizontal ? 260 : 50
            Behavior on height { NumberAnimation { duration: 500 } }
            width: listview.orientation == ListView.Horizontal ? 50 : 260
            Behavior on width { NumberAnimation { duration: 500 } }
            border.color: "black"
            Text {
                anchors.centerIn: parent; color: "black"; text: model.name
                rotation: listview.orientation == ListView.Horizontal ? -90 : 0
                Behavior on rotation { NumberAnimation { duration: 500 } }

            }
        }
    }

    Component {
        id: delegate2
        Rectangle {
            height: listview.orientation == ListView.Horizontal ? 260 : 50
            Behavior on height { NumberAnimation { duration: 500 } }
            width: listview.orientation == ListView.Horizontal ? 50 : 260
            Behavior on width { NumberAnimation { duration: 500 } }
            color: "goldenrod"; border.color: "black"
            Text {
                anchors.centerIn: parent; color: "royalblue"; text: model.name
                rotation: listview.orientation == ListView.Horizontal ? -90 : 0
                Behavior on rotation { NumberAnimation { duration: 1500 } }
            }
        }

    }

    Column {
        Rectangle {
            height: 50
            width: 50
            color: "blue"
            border.color: "orange"
            Text {
                anchors.centerIn: parent
                text: "Mod"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.model = listview.model == model2 ? model1 : model2
            }
        }

        Rectangle {
            height: 50
            width: 50
            color: "blue"
            border.color: "orange"
            Text {
                anchors.centerIn: parent
                text: "Del"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.delegate = listview.delegate == delegate2 ? delegate1 : delegate2
            }
        }

        Rectangle {
            height: 50
            width: 50
            color: "blue"
            border.color: "orange"
            Text {
                anchors.centerIn: parent
                text: "Ori"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.orientation = listview.orientation == ListView.Horizontal ? ListView.Vertical : ListView.Horizontal
            }
        }
    }

    ListModel {
        id: model1
        ListElement { name: "model1_1" }
        ListElement { name: "model1_2" }
        ListElement { name: "model1_3" }
        ListElement { name: "model1_4" }
        ListElement { name: "model1_5" }
    }

    ListModel {
        id: model2
        ListElement { name: "model2_1" }
        ListElement { name: "model2_2" }
        ListElement { name: "model2_3" }
        ListElement { name: "model2_4" }
        ListElement { name: "model2_5" }
    }
}
