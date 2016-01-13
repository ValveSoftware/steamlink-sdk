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
import QtQuick.Particles 2.0
import "content"

Item {
    id: root
    height: 480
    width: 320
    Item {
        id: startScreen
        anchors.fill: parent
        z: 1000
        Image {
            source: "content/title.png"
            anchors.centerIn: parent
        }
        MouseArea{
            anchors.fill: parent
            onClicked: {//Game Start
                parent.visible = false;
            }
        }
    }
    Rectangle {
        id: bg
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "DarkBlue" }
            GradientStop { position: 0.8; color: "SkyBlue" }
            GradientStop { position: 0.81; color: "ForestGreen" }
            GradientStop { position: 1.0; color: "DarkGreen" }
        }
    }

    BearWhackParticleSystem {
        id: particleSystem
        anchors.fill: parent
        running: !startScreen.visible
    }

    property int score: 0

    Text {
        anchors.right: parent.right
        anchors.margins: 4
        anchors.top: parent.top
        color: "white"
        function padded(num) {
            var ret = num.toString();
            while (ret.length < 6)
                ret = "0" + ret;
            return ret;
        }
        text: "Score: " + padded(score)
    }
    MultiPointTouchArea {
        anchors.fill: parent
        touchPoints: [//Support up to 4 touches at once?
            AugmentedTouchPoint{ system: particleSystem },
            AugmentedTouchPoint{ system: particleSystem },
            AugmentedTouchPoint{ system: particleSystem },
            AugmentedTouchPoint{ system: particleSystem }
        ]
    }
    MouseArea{
        anchors.fill: parent
        id: ma
        onPressedChanged: {
            if (pressed) {
                timer.restart();
                sgoal.enabled = true;
                particleSystem.explode(mouseX,mouseY);
            }
        }
        Timer {
            id: timer
            interval: 100
            running: false
            repeat: false
            onTriggered: sgoal.enabled = false
        }
        SpriteGoal {
            id: sgoal
            x: ma.mouseX - 16
            y: ma.mouseY - 16
            width: 32
            height: 32
            system: particleSystem
            parent: particleSystem
            goalState: "falling"
            enabled: false
        }
    }
}
