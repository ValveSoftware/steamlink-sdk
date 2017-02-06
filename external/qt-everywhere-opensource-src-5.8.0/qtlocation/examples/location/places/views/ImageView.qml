/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.5
import QtLocation 5.6
import QtQuick.Controls 1.4

Item {
    id: root
    property Place place
    width: parent.width
    height: parent.height

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

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.state = ""
                }
            }

            Button {
                id: button
                text: qsTr("Open url")
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    Qt.openUrlExternally(supplier.url)
                }
            }
        }
    }

    Label {
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
