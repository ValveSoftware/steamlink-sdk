/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtQuick 2.4
import QtQuick.Layouts 1.3

Item {
    property alias cancelButton: cancelButton
    property alias okButton: okButton
    property string message: "Message"
    property string title: "Title"
    property alias blue1: blue1
    property alias grid: grid
    property alias colorPicker: colorPicker

    ColumnLayout {
        id: columnLayout
        anchors.topMargin: 20
        anchors.top: parent.top
        anchors.bottomMargin: 20
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.right: parent.right
        anchors.leftMargin: 20
        anchors.left: parent.left

        Image {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            source: "qrc:/icon.svg"
        }

        Rectangle {
            height: 30
            anchors.rightMargin: 0
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.left: parent.left
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#25a6e2"
                }

                GradientStop {
                    color: "#188bd0"
                }
            }

            Text {
                id: title
                x: 54
                y: 5
                color: "#ffffff"
                text: qsTr("Select Color")
                font.pointSize: 12
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Item {
            width: 40
            height: 40
        }

        GridLayout {
            id: grid
            columns: 5
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            ColorCell {
                id: blue1
                color: "#26d5f8"
            }
            ColorCell {
                id: green1
                color: "#25f93d"
            }
            ColorCell {
                id: red1
                color: "#f71111"
            }
            ColorCell {
                id: yellow1
                color: "#faf23c"
            }
            ColorCell {
                id: orange1
                color: "#ec8505"
            }
            ColorCell {
                id: blue2
                color: "#037eaa"
            }
            ColorCell {
                id: green2
                color: "#389a13"
            }
            ColorCell {
                id: red2
                color: "#b2001b"
            }
            ColorCell {
                id: yellow2
                color: "#caca03"
            }
            ColorCell {
                id: orange2
                color: "#bb4900"
            }
            ColorCell {
                id: blue3
                color: "#01506c"
            }
            ColorCell {
                id: green3
                color: "#37592b"
            }
            ColorCell {
                id: red3
                color: "#700113"
            }
            ColorCell {
                id: yellow3
                color: "#848404"
            }

            ColorCell {
                id: orange3
                color: "#563100"
            }
        }

        Item {
            width: 10
            height: 10
        }

        Rectangle {
            width: 90
            height: 90
            color: "#000000"
            radius: 4
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            Rectangle {
                id: colorPicker
                height: 80
                color: "#ffffff"
                anchors.rightMargin: 5
                anchors.leftMargin: 5
                anchors.bottomMargin: 5
                anchors.topMargin: 5
                anchors.fill: parent
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            id: rowLayout
            width: 100
            height: 100

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: cancelButton
                width: 90
                height: 30
                btnText: qsTr("Cancel")
                btnBlue: false
            }

            Button {
                id: okButton
                width: 90
                height: 30
                btnText: qsTr("OK")
                btnBlue: false
            }
        }
    }
}
