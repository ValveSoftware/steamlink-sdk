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
import QtQml.Models 2.1

Item {
    id: root

    width: 320
    height: 480

    property bool dragging: false

    Component {
        id: packageDelegate
        Package {
            id: packageRoot

            MouseArea {
                id: visibleContainer
                Package.name: "visible"

                width: 64
                height: 64
                enabled: packageRoot.DelegateModel.inSelected

                drag.target: draggable

                Item {
                    id: draggable

                    width: 64
                    height: 64

                    Drag.active: visibleContainer.drag.active

                    anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter }

                    states: State {
                        when: visibleContainer.drag.active
                        AnchorChanges { target:  draggable; anchors { horizontalCenter: undefined; verticalCenter: undefined} }
                        ParentChange { target: selectionView; parent: draggable; x: 0; y: 0 }
                        PropertyChanges { target: root; dragging: true }
                        ParentChange { target: draggable; parent: root }
                    }
                }
                DropArea {
                    anchors.fill: parent
                    onEntered: selectedItems.move(0, visualModel.items.get(packageRoot.DelegateModel.itemsIndex), selectedItems.count)
                }
            }
            Item {
                id: selectionContainer
                Package.name: "selection"

                width: 64
                height: 64

                visible: PathView.onPath
            }
            Rectangle {
                id: content
                parent: visibleContainer

                width: 58
                height: 58

                radius: 8

                gradient: Gradient {
                    GradientStop { id: gradientStart; position: 0.0; color: "#8AC953" }
                    GradientStop { id: gradientEnd; position: 1.0; color: "#8BC953" }
                }

                border.width: 2
                border.color: "#007423"

                state: root.dragging && packageRoot.DelegateModel.inSelected ? "selected" : "visible"

                Text {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "white"
                    text: modelData
                    font.pixelSize: 18
                }

                Rectangle {
                    anchors { right: parent.right; top: parent.top; margins: 3 }
                    width: 12; height: 12
                    color: packageRoot.DelegateModel.inSelected ? "black" : "white"
                    radius: 6

                    border.color: "white"
                    border.width: 2

                    MouseArea {
                        anchors.fill: parent
                        onClicked: packageRoot.DelegateModel.inSelected = !packageRoot.DelegateModel.inSelected
                    }
                }

                states: [
                    State {
                        name: "selected"
                        ParentChange { target: content; parent: selectionContainer; x: 3; y: 3 }
                        PropertyChanges { target: packageRoot; DelegateModel.inItems: visibleContainer.drag.active }
                        PropertyChanges { target: gradientStart; color: "#017423" }
                        PropertyChanges { target: gradientStart; color: "#007423" }
                    }, State {
                        name: "visible"
                        PropertyChanges { target: packageRoot; DelegateModel.inItems: true }
                        ParentChange { target: content; parent: visibleContainer; x: 3; y: 3 }
                        PropertyChanges { target: gradientStart; color: "#8AC953" }
                        PropertyChanges { target: gradientStart; color: "#8BC953" }
                    }
                ]
                transitions: Transition {
                    PropertyAction { target: packageRoot; properties: "DelegateModel.inItems" }
                    ParentAnimation {
                        target: content
                        NumberAnimation { target: content; properties: "x,y"; duration: 500 }
                    }
                    ColorAnimation { targets: [gradientStart, gradientEnd]; duration: 500 }
                }
            }
        }
    }

    DelegateModel {
        id: visualModel
        model: 35
        delegate: packageDelegate

        groups: VisualDataGroup { id: selectedItems; name: "selected" }

        Component.onCompleted:  parts.selection.filterOnGroup = "selected"
    }

    PathView {
        id: selectionView

        height: 64
        width: 64

        model: visualModel.parts.selection

        path: Path {
            startX: 0
            startY: 0
            PathLine { x: 64; y: 64 }
        }
    }

    GridView {
        id: itemsView
        anchors { fill: parent }
        cellWidth: 64
        cellHeight: 64
        model: visualModel.parts.visible
    }
}
