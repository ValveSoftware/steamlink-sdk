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

import QtQuick 1.0

Item {
    id: root

    property alias background: background.source
    property int currentIndex: 0
    default property alias content: visualModel.children

    Image {
        id: background
        fillMode: Image.TileHorizontally
        x: -list.contentX / 2
        width: Math.max(list.contentWidth, parent.width)
    }

    ListView {
        id: list
        anchors.fill: parent

        currentIndex: root.currentIndex
        onCurrentIndexChanged: root.currentIndex = currentIndex

        orientation: Qt.Horizontal
        boundsBehavior: Flickable.DragOverBounds
        model: VisualItemModel { id: visualModel }

        highlightRangeMode: ListView.StrictlyEnforceRange
        snapMode: ListView.SnapOneItem
    }

    ListView {
        id: selector

        height: 50
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(count * 50, parent.width - 20)
        interactive: width == parent.width - 20
        orientation: Qt.Horizontal

        currentIndex: root.currentIndex
        onCurrentIndexChanged: root.currentIndex = currentIndex

        model: visualModel.children
        delegate: Item {
            width: 50; height: 50
            id: delegateRoot

            Image {
                id: image
                source: modelData.icon
                smooth: true
                scale: 0.8
            }

            MouseArea {
                anchors.fill: parent
                onClicked: { root.currentIndex = index }
            }

            states: State {
                name: "Selected"
                when: delegateRoot.ListView.isCurrentItem == true
                PropertyChanges {
                    target: image
                    scale: 1
                    y: -5
                }
            }
            transitions: Transition {
                NumberAnimation { properties: "scale,y" }
            }
        }

        Rectangle {
            color: "#60FFFFFF"
            x: -10; y: -10; z: -1
            width: parent.width + 20; height: parent.height + 20
            radius: 10
        }
    }
}
