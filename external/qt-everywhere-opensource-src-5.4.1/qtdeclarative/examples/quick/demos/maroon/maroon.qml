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
import "content/logic.js" as Logic

Item {
    id: root
    width: 320
    height: 480
    property var gameState
    property bool passedSplash: false

    Image {
        source:"content/gfx/background.png"
        anchors.bottom: view.bottom

        ParticleSystem {
            id: particles
            anchors.fill: parent

            ImageParticle {
                id: bubble
                anchors.fill: parent
                source: "content/gfx/catch.png"
                opacity: 0.25
            }

            Wander {
                xVariance: 25;
                pace: 25;
            }

            Emitter {
                width: parent.width
                height: 150
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                startTime: 15000

                emitRate: 2
                lifeSpan: 15000

                acceleration: PointDirection{ y: -6; xVariation: 2; yVariation: 2 }

                size: 24
                sizeVariation: 16
            }
        }
    }

    Column {
        id: view
        y: -(height - 480)
        width: 320

        GameOverScreen { gameCanvas: canvas }

        Item {
            id: canvasArea
            width: 320
            height: 480

            Row {
                height: childrenRect.height
                Image {
                    id: wave
                    y: 30
                    source:"content/gfx/wave.png"
                }
                Image {
                    y: 30
                    source:"content/gfx/wave.png"
                }
                NumberAnimation on x { from: 0; to: -(wave.width); duration: 16000; loops: Animation.Infinite }
                SequentialAnimation on y {
                    loops: Animation.Infinite
                    NumberAnimation { from: y - 2; to: y + 2; duration: 1600; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: y + 2; to: y - 2; duration: 1600; easing.type: Easing.InOutQuad }
                }
            }

            Row {
                opacity: 0.5
                Image {
                    id: wave2
                    y: 25
                    source: "content/gfx/wave.png"
                }
                Image {
                    y: 25
                    source: "content/gfx/wave.png"
                }
                NumberAnimation on x { from: -(wave2.width); to: 0; duration: 32000; loops: Animation.Infinite }
                SequentialAnimation on y {
                    loops: Animation.Infinite
                    NumberAnimation { from: y + 2; to: y - 2; duration: 1600; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: y - 2; to: y + 2; duration: 1600; easing.type: Easing.InOutQuad }
                }
            }

            Image {
                source: "content/gfx/sunlight.png"
                opacity: 0.02
                y: 0
                anchors.horizontalCenter: parent.horizontalCenter
                transformOrigin: Item.Top
                SequentialAnimation on rotation {
                    loops: Animation.Infinite
                    NumberAnimation { from: -10; to: 10; duration: 8000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 10; to: -10; duration: 8000; easing.type: Easing.InOutSine }
                }
            }

            Image {
                source: "content/gfx/sunlight.png"
                opacity: 0.04
                y: 20
                anchors.horizontalCenter: parent.horizontalCenter
                transformOrigin: Item.Top
                SequentialAnimation on rotation {
                    loops: Animation.Infinite
                    NumberAnimation { from: 10; to: -10; duration: 8000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: -10; to: 10; duration: 8000; easing.type: Easing.InOutSine }
                }
            }

            Image {
                source: "content/gfx/grid.png"
                opacity: 0.5
            }

            GameCanvas {
                id: canvas
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                x: 32
                focus: true
            }

            InfoBar { anchors.bottom: canvas.top; anchors.bottomMargin: 6; width: parent.width }

            //3..2..1..go
            Timer {
                id: countdownTimer
                interval: 1000
                running: root.countdown < 5
                repeat: true
                onTriggered: root.countdown++
            }
            Repeater {
                model: ["content/gfx/text-blank.png", "content/gfx/text-3.png", "content/gfx/text-2.png", "content/gfx/text-1.png", "content/gfx/text-go.png"]
                delegate: Image {
                    visible: root.countdown <= index
                    opacity: root.countdown == index ? 0.5 : 0.1
                    scale: root.countdown >= index ? 1.0 : 0.0
                    source: modelData
                    Behavior on opacity { NumberAnimation {} }
                    Behavior on scale { NumberAnimation {} }
                }
            }
        }

        NewGameScreen {
            onStartButtonClicked: root.passedSplash = true
        }
    }

    property int countdown: 10
    Timer {
        id: gameStarter
        interval: 4000
        running: false
        repeat: false
        onTriggered: Logic.startGame(canvas);
    }

    states: [
        State {
            name: "gameOn"; when: gameState.gameOver == false && passedSplash
            PropertyChanges { target: view; y: -(height - 960) }
            StateChangeScript { script: root.countdown = 0; }
            PropertyChanges { target: gameStarter; running: true }
        },
        State {
            name: "gameOver"; when: gameState.gameOver == true
            PropertyChanges { target: view; y: 0 }
        }
    ]

    transitions: Transition {
        NumberAnimation { properties: "x,y"; duration: 1200; easing.type: Easing.OutQuad }
    }

    Component.onCompleted: gameState = Logic.newGameState(canvas);
}
