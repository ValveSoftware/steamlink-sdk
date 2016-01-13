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
import QtLocation 5.3

Item {
    id: root

    anchors.fill: parent

    clip: true

    GridView {
        id: gridView

        anchors.fill: parent

        model: place.imageModel

        cellWidth: width / 3
        cellHeight: cellWidth

        delegate: Rectangle {
            width: gridView.cellWidth
            height: gridView.cellHeight

            color: "#30FFFFFF"

            Image {
                anchors.fill: parent
                anchors.margins: 5

                source: url

                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.positionViewAtIndex(index, ListView.Contain);
                    root.state = "list";
                }
            }
        }
    }

    ListView {
        id: listView

        anchors.top: parent.top
        anchors.bottom: position.top
        width: parent.width
        spacing: 10

        model: place.imageModel
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem

        visible: false

        delegate: Item {
            width: listView.width
            height: listView.height

            Image {
                anchors.fill: parent

                source: url

                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: supplier.name + "\n" + supplier.url
                width: parent.width
                anchors.bottom: parent.bottom
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.state = ""
        }
    }

    Text {
        id: position

        width: parent.width
        anchors.bottom: parent.bottom
        visible: listView.visible

        text: (listView.currentIndex + 1) + '/' + listView.model.totalCount
        horizontalAlignment: Text.AlignRight
    }

    states: [
        State {
            name: "list"
            PropertyChanges {
                target: gridView
                visible: false
            }
            PropertyChanges {
                target: listView
                visible: true
            }
        }
    ]
}
