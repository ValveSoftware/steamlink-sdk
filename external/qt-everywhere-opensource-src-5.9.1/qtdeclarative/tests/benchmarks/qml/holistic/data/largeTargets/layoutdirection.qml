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

import QtQuick 2.0

Rectangle {

    width: column.width + 100
    height: column.height + 100
    property int direction: Qt.application.layoutDirection

    Column {
        id: column
        spacing: 10
        anchors.centerIn: parent
        width: 230

        Text {
            text: "Row"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Row {
            layoutDirection: direction
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
                    sourceComponent: delegate
                }
            }
        }
        Text {
            text: "Grid"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Grid {
           layoutDirection: direction
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
                   sourceComponent: delegate
               }
            }
        }
        Text {
            text: "Flow"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Flow {
           layoutDirection: direction
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
                   sourceComponent: delegate
               }
            }
        }
        Rectangle {
           height: 50; width: parent.width
           color: mouseArea.pressed ? "black" : "gray"
           Text {
                text: direction ? "Right to left" : "Left to right"
                color: "white"
                font.pixelSize: 16
                anchors.centerIn: parent
            }
            MouseArea {
                id: mouseArea
                onClicked: {
                    if (direction == Qt.LeftToRight) {
                        direction = Qt.RightToLeft;
                    } else {
                        direction = Qt.LeftToRight;
                    }
                }
                anchors.fill: parent
            }
        }
    }

    Component {
        id: delegate
        Rectangle {
            width: 50; height: 50
            color: Qt.rgba(0.8/(parent.value+1),0.8/(parent.value+1),0.8/(parent.value+1),1.0)
            Text {
                text: parent.parent.value+1
                color: "white"
                font.pixelSize: 20
                anchors.centerIn: parent
            }
        }
    }
}
