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

import QtQuick 1.1

Rectangle {
    id: root
    property bool mirror
    property int direction: Qt.application.layoutDirection
    LayoutMirroring.enabled: mirror
    LayoutMirroring.childrenInherit: true
    width: column.width + 80
    height: column.height + 40
    Column {
        id: column
        width: 190
        spacing: 10
        anchors.centerIn: parent

        Text {
            text: "Row"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            layoutDirection: root.direction
            spacing: 10
            move: Transition {
                NumberAnimation {
                    properties: "x"
                }
            }
            Repeater {
                model: 4
                Loader {
                    property int value: index
                    sourceComponent: positionerDelegate
                }
            }
        }

        Text {
            text: "Grid"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Grid {
            layoutDirection: root.direction
            spacing: 10; columns: 4
            move: Transition {
                NumberAnimation {
                    properties: "x"
                }
            }
            Repeater {
                model: 11
                Loader {
                    property int value: index
                    sourceComponent: positionerDelegate
                }
            }
        }

        Text {
            text: "Flow"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Flow {
            layoutDirection: root.direction
            spacing: 10; width: parent.width
            move: Transition {
                NumberAnimation {
                    properties: "x"
                }
            }
            Repeater {
                model: 10
                Loader {
                    property int value: index
                    sourceComponent: positionerDelegate
                }
            }
        }

        Text {
            text: "ListView"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        ListView {
            id: listView
            clip: true
            width: parent.width; height: 40
            layoutDirection: root.direction
            orientation: Qt.Horizontal
            model: 48
            delegate: viewDelegate
        }

        Text {
            text: "GridView"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        GridView {
            clip: true
            width: 200; height: 160
            cellWidth: 50; cellHeight: 50
            layoutDirection: root.direction
            model: 48
            delegate: viewDelegate
        }

        Rectangle {
            height: 50; width: parent.width
            color: mouseArea.pressed ? "black" : "gray"
            Column {
                anchors.centerIn: parent
                Text {
                    text: root.direction ? "Right to left" : "Left to right"
                    color: "white"
                    font.pixelSize: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: "(click here to toggle)"
                    color: "white"
                    font.pixelSize: 10
                    font.italic: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            MouseArea {
                id: mouseArea
                anchors.fill: parent
                onClicked: {
                    if (root.direction == Qt.LeftToRight) {
                        root.direction = Qt.RightToLeft;
                    } else {
                        root.direction = Qt.LeftToRight;
                    }
                }
            }
        }

        Rectangle {
            height: 50; width: parent.width
            color: mouseArea2.pressed ? "black" : "gray"
            Column {
                anchors.centerIn: parent
                Text {
                    text: root.mirror ? "Mirrored" : "Not mirrored"
                    color: "white"
                    font.pixelSize: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: "(click here to toggle)"
                    color: "white"
                    font.pixelSize: 10
                    font.italic: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            MouseArea {
                id: mouseArea2
                anchors.fill: parent
                onClicked: {
                    root.mirror = !root.mirror;
                }
            }
        }
    }

    Component {
        id: positionerDelegate
        Rectangle {
            width: 40; height: 40
            color: Qt.rgba(0.8/(parent.value+1),0.8/(parent.value+1),0.8/(parent.value+1),1.0)
            Text {
                text: parent.parent.value+1
                color: "white"
                font.pixelSize: 18
                anchors.centerIn: parent
            }
        }
    }
    Component {
        id: viewDelegate
        Item {
            function effectiveLayoutDirection() {
                if (LayoutMirroring.enabled)
                    if (listView.layoutDirection == Qt.LeftToRight)
                        return Qt.RightToLeft;
                    else
                        return Qt.LeftToRight;
                else
                    return listView.layoutDirection;
            }

            width: (effectiveLayoutDirection() == Qt.LeftToRight ? (index == 48 - 1) : (index == 0)) ? 40 : 50
            Rectangle {
                width: 40; height: 40
                color: Qt.rgba(0.5+(48 - index)*Math.random()/48,
                               0.3+index*Math.random()/48,
                               0.3*Math.random(),
                               1.0)
                Text {
                    text: index+1
                    color: "white"
                    font.pixelSize: 18
                    anchors.centerIn: parent
                }
            }
        }
    }
}

