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
import QtQml.Models 2.1

ListView {
    id: listview

    width: 600
    height: 200

    spacing: 8
    orientation: ListView.Horizontal

    Component.onCompleted: {
        var colors = ["blue", "green", "red", "yellow", "orange", "purple", "cyan",
                      "magenta", "chartreuse", "aquamarine", "indigo", "lightsteelblue",
                      "violet", "grey", "springgreen", "salmon", "blanchedalmond",
                      "forestgreen", "pink", "navy", "goldenrod", "crimson", "teal" ]
        for (var i = 0; i < 100; ++i)
            colorModel.append( { nid: i,  color: colors[i%colors.length] } )
    }

    model: DelegateModel {
        id: visualModel

        model: ListModel {
            id: colorModel
        }

        delegate: MouseArea {
            id: delegateRoot
            objectName: model.nid

            width: 107.35
            height: 63.35

            drag.target: icon

            Rectangle{
                id: icon
                width: delegateRoot.width
                height: delegateRoot.height
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: model.color

                Drag.active: delegateRoot.drag.active
                Drag.source: delegateRoot
                Drag.hotSpot.x: 36
                Drag.hotSpot.y: 36

                Text {
                    id: text
                    anchors.fill: parent
                    font.pointSize: 40
                    text: model.nid
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }

                states: [
                    State {
                        when: icon.Drag.active
                        ParentChange {
                            target: icon
                            parent: delegateRoot.ListView.view
                        }

                        AnchorChanges {
                            target: icon
                            anchors.horizontalCenter: undefined
                            anchors.verticalCenter: undefined
                        }
                    }
                ]
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onPositionChanged: {
            var to = listview.indexAt(drag.x + listview.contentX, 0)
            if (to !== -1) {
                var from = drag.source.DelegateModel.itemsIndex
                if (from !== to)
                    visualModel.items.move(from, to)
                drag.accept()
            }
        }
    }
}
