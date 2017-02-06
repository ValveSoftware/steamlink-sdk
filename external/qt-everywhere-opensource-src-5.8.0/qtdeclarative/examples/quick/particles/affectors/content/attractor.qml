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
import QtQuick.Particles 2.0

Rectangle {
    id: root
    width: 360
    height: 540
    color: "black"
    Image {
        source: "../../images/finalfrontier.png"
        anchors.centerIn:parent
    }
    ParticleSystem {
        id: particles
        anchors.fill: parent

        Emitter {
            group: "stars"
            emitRate: 40
            lifeSpan: 4000
            enabled: true
            size: 30
            sizeVariation: 10
            velocity: PointDirection { x: 220; xVariation: 40 }
            height: parent.height
        }
        Emitter {
            group: "roids"
            emitRate: 10
            lifeSpan: 4000
            enabled: true
            size: 30
            sizeVariation: 10
            velocity: PointDirection { x: 220; xVariation: 40 }
            height: parent.height
        }
        ImageParticle {
            id: stars
            groups: ["stars"]
            source: "qrc:///particleresources/star.png"
            color: "white"
            colorVariation: 0.1
            alpha: 0
        }
        ImageParticle {
            id: roids
            groups: ["roids"]
            sprites: Sprite {
                id: spinState
                name: "spinning"
                source: "../../images/meteor.png"
                frameCount: 35
                frameDuration: 60
            }
        }
        ImageParticle {
            id: shot
            groups: ["shot"]
            source: "qrc:///particleresources/star.png"

            color: "#0FF06600"
            colorVariation: 0.3
        }
        ImageParticle {
            id: engine
            groups: ["engine"]
            source: "qrc:///particleresources/fuzzydot.png"

            color: "orange"
            SequentialAnimation on color {
                loops: Animation.Infinite
                ColorAnimation {
                    from: "red"
                    to: "cyan"
                    duration: 1000
                }
                ColorAnimation {
                    from: "cyan"
                    to: "red"
                    duration: 1000
                }
            }

            colorVariation: 0.2
        }
        //! [0]
        Attractor {
            id: gs; pointX: root.width/2; pointY: root.height/2; strength: 4000000;
            affectedParameter: Attractor.Acceleration
            proportionalToDistance: Attractor.InverseQuadratic
        }
        //! [0]
        Age {
            x: gs.pointX - 8;
            y: gs.pointY - 8;
            width: 16
            height: 16
        }
        Rectangle {
            color: "black"
            width: 8
            height: 8
            radius: 4
            x: gs.pointX - 4
            y: gs.pointY - 4
        }

        Image {
            source:"../../images/rocket2.png"
            id: ship
            width: 45
            height: 22
            //Automatic movement
            SequentialAnimation on x {
                loops: -1
                NumberAnimation{to: root.width-45; easing.type: Easing.InOutQuad; duration: 2000}
                NumberAnimation{to: 0; easing.type: Easing.OutInQuad; duration: 6000}
            }
            SequentialAnimation on y {
                loops: -1
                NumberAnimation{to: root.height-22; easing.type: Easing.OutInQuad; duration: 6000}
                NumberAnimation{to: 0; easing.type: Easing.InOutQuad; duration: 2000}
            }
        }
        Emitter {
            group: "engine"
            emitRate: 200
            lifeSpan: 1000
            size: 10
            endSize: 4
            sizeVariation: 4
            velocity: PointDirection { x: -128; xVariation: 32 }
            height: ship.height
            y: ship.y
            x: ship.x
            width: 20
        }
        Emitter {
            group: "shot"
            emitRate: 32
            lifeSpan: 1000
            enabled: true
            size: 40
            velocity: PointDirection { x: 256; }
            x: ship.x + ship.width
            y: ship.y + ship.height/2
        }
    }
}

