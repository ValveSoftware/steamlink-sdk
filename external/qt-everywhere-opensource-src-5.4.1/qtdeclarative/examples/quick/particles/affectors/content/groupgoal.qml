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


Rectangle {
    id: root
    width: 360
    height: 600
    color: "black"

    property int score: 0
    Text {
        color: "white"
        anchors.right: parent.right
        text: score
    }

    ParticleSystem {
        id: particles
        anchors.fill: parent
        // ![unlit]
        ParticleGroup {
            name: "unlit"
            duration: 1000
            to: {"lighting":1, "unlit":99}
            ImageParticle {
                source: "../../images/particleA.png"
                colorVariation: 0.1
                color: "#2060160f"
            }
            GroupGoal {
                whenCollidingWith: ["lit"]
                goalState: "lighting"
                jump: true
            }
        }
        // ![unlit]
        // ![lighting]
        ParticleGroup {
            name: "lighting"
            duration: 100
            to: {"lit":1}
        }
        // ![lighting]
        // ![lit]
        ParticleGroup {
            name: "lit"
            duration: 10000
            onEntered: score++;
            TrailEmitter {
                id: fireballFlame
                group: "flame"

                emitRatePerParticle: 48
                lifeSpan: 200
                emitWidth: 8
                emitHeight: 8

                size: 24
                sizeVariation: 8
                endSize: 4
            }

            TrailEmitter {
                id: fireballSmoke
                group: "smoke"
        // ![lit]

                emitRatePerParticle: 120
                lifeSpan: 2000
                emitWidth: 16
                emitHeight: 16

                velocity: PointDirection {yVariation: 16; xVariation: 16}
                acceleration: PointDirection {y: -16}

                size: 24
                sizeVariation: 8
                endSize: 8
            }
        }

        ImageParticle {
            id: smoke
            anchors.fill: parent
            groups: ["smoke"]
            source: "qrc:///particleresources/glowdot.png"
            colorVariation: 0
            color: "#00111111"
        }
        ImageParticle {
            id: pilot
            anchors.fill: parent
            groups: ["pilot"]
            source: "qrc:///particleresources/glowdot.png"
            redVariation: 0.01
            blueVariation: 0.4
            color: "#0010004f"
        }
        ImageParticle {
            id: flame
            anchors.fill: parent
            groups: ["flame", "lit", "lighting"]
            source: "../../images/particleA.png"
            colorVariation: 0.1
            color: "#00ff400f"
        }

        Emitter {
            height: parent.height/2
            emitRate: 4
            lifeSpan: 4000//TODO: Infinite & kill zone
            size: 24
            sizeVariation: 4
            velocity: PointDirection {x:120; xVariation: 80; yVariation: 50}
            acceleration: PointDirection {y:120}
            group: "unlit"
        }

        Emitter {
            id: flamer
            x: 100
            y: 300
            group: "pilot"
            emitRate: 80
            lifeSpan: 600
            size: 24
            sizeVariation: 2
            endSize: 0
            velocity: PointDirection { y:-100; yVariation: 4; xVariation: 4 }
            // ![groupgoal-pilot]
            GroupGoal {
                groups: ["unlit"]
                goalState: "lit"
                jump: true
                system: particles
                x: -15
                y: -55
                height: 75
                width: 30
                shape: MaskShape {source: "../../images/matchmask.png"}
            }
            // ![groupgoal-pilot]
        }
        // ![groupgoal-ma]
        //Click to enflame
        GroupGoal {
            groups: ["unlit"]
            goalState: "lighting"
            jump: true
            enabled: ma.pressed
            width: 18
            height: 18
            x: ma.mouseX - width/2
            y: ma.mouseY - height/2
        }
        // ![groupgoal-ma]
        MouseArea {
            id: ma
            anchors.fill: parent
        }
    }
}
