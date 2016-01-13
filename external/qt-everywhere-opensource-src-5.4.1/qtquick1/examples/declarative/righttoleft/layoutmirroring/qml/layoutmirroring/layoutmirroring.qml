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
    property bool mirror: Qt.application.layoutDirection == Qt.RightToLeft
    LayoutMirroring.enabled: mirror
    LayoutMirroring.childrenInherit: true
    width: 400
    height: 875
    color: "lightsteelblue"

    Column {
        spacing: 10
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }

        Text {
            text: "Positioners"
            anchors.left: parent.left
        }

        Column {
            id: positioners
            spacing: 5
            anchors.left: parent.left
            Row {
                id: row
                spacing: 4
                property string text: "THISISROW"
                anchors.left: parent.left
                Repeater {
                    model: parent.text.length
                    delegate: positionerDelegate
                }
            }
            Flow {
                id: flow
                spacing: 4
                width: 90
                property string text: "THISISFLOW"
                anchors.left: parent.left
                Repeater {
                    model: parent.text.length
                    delegate: positionerDelegate
                }
            }
            Grid {
                id: grid
                spacing: 4
                columns: 6
                property string text: "THISISGRID"
                anchors.left: parent.left
                Repeater {
                    model: parent.text.length
                    delegate: positionerDelegate
                }
            }
            Component {
                id: positionerDelegate
                Text {
                    color: "white"
                    font.pixelSize: 20
                    text: parent.text[index]
                    Rectangle {
                        z: -1
                        opacity: 0.7
                        color: "black"
                        anchors.fill: parent
                    }
                }
            }
        }

        Text {
            text: "Text alignment"
            anchors.left: parent.left
        }

        Rectangle {
            id: textStrings
            width: 148
            height: 85
            color: "white"
            anchors.left: parent.left
            Column {
                spacing: 5
                width: parent.width
                anchors { fill: parent; margins: 5 }
                Text {
                    id: englishText
                    width: parent.width
                    text: "English text"
                }
                Text {
                    id: arabicText
                    width: parent.width
                    text: "النص العربي"
                }
                Text {
                    id: leftAlignedText
                    width: parent.width
                    text: "Text aligned to left"
                    horizontalAlignment: Text.AlignLeft
                }
                Text {
                    id: rightAlignedText
                    width: parent.width
                    text: "Text aligned to right"
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        Text {
            text: "Model views"
            anchors.left: parent.left
        }

        Column {
            id: views
            spacing: 10
            anchors.left: parent.left
            ListView {
                id: listView
                z: -1
                clip: true
                model: text.length
                width: 360; height: 45
                orientation: Qt.Horizontal
                property string text: "LISTVIEWLISTVIEWLISTVIEWLISTVIEWLISTVIEWLISTVIEW"
                delegate: Rectangle {
                    color: "black"
                    width: 45; height: 45
                    Rectangle {
                        anchors { fill: parent; margins: 1 }
                        color: "red"
                    }
                    Text {
                        text: listView.text[index]
                        font.pixelSize: 30
                        anchors.centerIn: parent
                    }
                }
            }
            GridView {
                id: gridView
                z: -1
                clip: true
                model: text.length
                width: 180; height: 90
                cellWidth: 45; cellHeight: 45
                property string text: "GRIDVIEWGRIDVIEWGRIDVIEWGRIDVIEWGRIDVIEWGRIDVIEW"
                anchors.left: parent.left
                delegate: Rectangle {
                    color: "black"
                    width: 45; height: 45
                    Rectangle {
                        anchors { fill: parent; margins: 1 }
                        color: "red"
                    }
                    Text {
                        anchors.centerIn: parent
                        font.pixelSize: 30
                        text: gridView.text[index]
                    }
                }
            }
        }

        Text {
            text: "Item x"
            anchors.left: parent.left
        }
        Rectangle {
            id: items
            color: Qt.rgba(0.2, 0.2, 0.2, 0.6)
            width: 275; height: 95
            anchors.left: parent.left
            Rectangle {
                y: 5; x: 5
                width: 130; height: 40
                Text {
                    text: "Item with x: 5\n(not mirrored)"
                    anchors.centerIn: parent
                }
            }
            Rectangle {
                color:  Qt.rgba(0.7, 0.7, 0.7)
                y: 50; x: mirror(5)
                width: 130; height: 40
                function mirror(value) {
                    return LayoutMirroring.enabled ? (parent.width - width - value) : value;
                }
                Text {
                    text: "Item with x: " + parent.x + "\n(manually mirrored)"
                    anchors.centerIn: parent
                }
            }
        }
        Text {
            text: "Item anchors"
            anchors.left: parent.left
        }

        Rectangle {
            id: anchoredItems
            color: Qt.rgba(0.2, 0.2, 0.2, 0.6)
            width: 270; height: 170
            anchors.left: parent.left
            Rectangle {
                id: blackRectangle
                color: "black"
                width: 180; height: 90
                anchors { horizontalCenter: parent.horizontalCenter; horizontalCenterOffset: 30 }
                Text {
                    text: "Horizontal center anchored\nwith offset 30\nto the horizontal center\nof the parent."
                    color: "white"
                    anchors.centerIn: parent
                }
            }
            Rectangle {
                id: whiteRectangle
                color: "white"
                width: 120; height: 70
                anchors { left: parent.left; bottom: parent.bottom }
                Text {
                    text: "Left side anchored\nto the left side\nof the parent."
                    color: "black"
                    anchors.centerIn: parent
                }
            }
            Rectangle {
                id: grayRectangle
                color: Qt.rgba(0.7, 0.7, 0.7)
                width: 140; height: 90
                anchors { right: parent.right; bottom: parent.bottom }
                Text {
                    text: "Right side anchored\nto the right side\nof the parent."
                    anchors.centerIn: parent
                }
            }
        }
        Rectangle {
            id: mirrorButton
            color: mouseArea2.pressed ? "black" : "gray"
            height: 50; width: parent.width
            anchors.left: parent.left
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
}

