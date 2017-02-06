/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

Item {
    Rectangle {
        color: "lightGray"
        anchors.fill: parent
        anchors.margins: 10

        Row {
            anchors.fill: parent
            anchors.margins: 10
            Rectangle {
                color: "red"
//                ColorAnimation on color {
//                    from: "black"
//                    to: "white"
//                    duration: 2000
//                    loops: Animation.Infinite
//                }
                width: 300
                height: 100
                layer.enabled: true
                Text { text: "this is in a layer, going through an offscreen render target" }
                clip: true
                Rectangle {
                    color: "lightGreen"
                    width: 50
                    height: 50
                    x: 275
                    y: 75
                }
            }
            Rectangle {
                color: "white"
                width: 300
                height: 100
                Text { text: "this is not a layer" }
            }
            Rectangle {
                color: "green"
                width: 300
                height: 100
                layer.enabled: true
                Text { text: "this is another layer" }
                Rectangle {
                    border.width: 4
                    border.color: "black"
                    anchors.centerIn: parent
                    width: 150
                    height: 50
                    layer.enabled: true
                    Text {
                        anchors.centerIn: parent
                        text: "layer in a layer"
                    }
                }
                Image {
                    source: "qrc:/face-smile.png"
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    NumberAnimation on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite; }
                }
            }
        }
    }
}
