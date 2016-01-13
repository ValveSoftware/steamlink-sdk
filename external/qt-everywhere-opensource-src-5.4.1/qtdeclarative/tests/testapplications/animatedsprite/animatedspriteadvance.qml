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

    width: 320
    height: 480
    color: "lightgray"
    Column {
        Text { text: "Current frame: "+s1.currentFrame }
        Text { text: "Delay: " + animcontroller.interval + "msec" }
    }

    Row {
        height: parent.height / 3
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        spacing: 5
        Column {
            height: parent.height
            width: 80
            Text { id: bodytxt; width: parent.width; text: "Body"; horizontalAlignment: Text.AlignHCenter }
            Rectangle {
                height: 30
                width: parent.width
                color: "white"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s1.source = "bear_polar.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "black"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s1.source = "bear_black.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "brown"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s1.source = "bear_brown.png" }
                }
            }
        }
        Column {
            height: parent.height
            width: 80
            Text { id: eyestxt; width: parent.width; text: "Eyes"; horizontalAlignment: Text.AlignHCenter }
            Rectangle {
                height: 30
                width: parent.width
                color: "brown"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s2.source = "bear_eyes_brown.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "blue"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s2.source = "bear_eyes_blue.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "green"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s2.source = "bear_eyes_green.png" }
                }
            }
        }
        Column {
            height: parent.height
            width: 80
            Text { id: furtxt; width: parent.width; text: "Fur"; horizontalAlignment: Text.AlignHCenter }
            Rectangle {
                height: 30
                width: parent.width
                color: "orange"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s3.source = "bear_fur_orange.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "gray"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s3.source = "bear_fur_gray.png" }
                }
            }
            Rectangle {
                height: 30
                width: parent.width
                color: "magenta"
                MouseArea {
                    anchors.fill: parent
                    onClicked: { s3.source = "bear_fur_pink.png" }
                }
            }
        }
    }

    function advanceall() {
        s1.advance();
        s2.advance();
        s3.advance();
        if (s2.currentFrame!=s1.currentFrame || s3.currentFrame!=s1.currentFrame)
            console.log("Frames out of sync!")
    }

    AnimatedSprite {
        id: s1
        x: 100
        y: 100
        running: true
        paused: true
        height: 125
        width: 125
        frameCount: 13
        interpolate: false
        source: "bear_brown.png"
    }

    AnimatedSprite {
        id: s2
        anchors.fill: s1
        running: true
        paused: s1.paused
        frameCount: 13
        interpolate: false
        source: "bear_eyes_brown.png"
        onSourceChanged: { currentFrame = s1.currentFrame }
    }

    AnimatedSprite {
        id: s3
        anchors.fill: s1
        running: true
        paused: s1.paused
        frameCount: 13
        interpolate: false
        source: "bear_fur_orange.png"
        onSourceChanged: { currentFrame = s1.currentFrame }
    }

    Timer { id: animcontroller; interval: 75; running: true; repeat: true; onTriggered: { advanceall(); } }
}
