/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

Rectangle {
    id: main

    property bool reversed: false
    property real speed: 5
    property bool framesync: false

    width: 320
    height: 480
    color: "lightgray"

    Column {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        width: parent.width - 10
        spacing: 5
        Text {
            text: framesync ? "Rate: FrameSync" : "Rate: FrameRate"
            width: controls.width
            height: 50
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            MouseArea {
                anchors.fill: parent
                onClicked: framesync = !framesync
            }
            Rectangle { anchors.fill: parent; color: "transparent"; border.color: "black"; radius: 5 }
        }
        Text {
            text:  reversed ? "Reverse" : "Forward"
            width: controls.width
            height: 50
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            MouseArea {
                anchors.fill: parent
                onClicked: reversed = !reversed
            }
            Rectangle { anchors.fill: parent; color: "transparent"; border.color: "black"; radius: 5 }
        }

        Text {
            text: "FPS: "+s1.frameRate
            width: controls.width
            height: 50
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            Rectangle {
                height: parent.height
                width: height
                Text { anchors.centerIn: parent; text: "-" }
                MouseArea {
                    anchors.fill: parent
                    onClicked: speed = speed - 1
                }
            }
            Rectangle {
                height: parent.height
                width: height
                anchors.right: parent.right
                Text { anchors.centerIn: parent; text: "+" }
                MouseArea {
                    anchors.fill: parent
                    onClicked: speed = speed + 1
                }
            }
            Rectangle { anchors.fill: parent; color: "transparent"; border.color: "black"; radius: 5 }
        }
    }

    AnimatedSprite {
        id: s1
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -80
        running: true
        height: 125
        width: 125
        frameCount: 13
        frameDuration: 50
        frameRate: speed
        frameSync: framesync
        reverse: reversed
        interpolate: false
        source: "bear_tiles.png"
    }
}
