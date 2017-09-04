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

    ParticleSystem {
        id: particles
        anchors.fill: parent

        ImageParticle {
            id: smoke
            system: particles
            anchors.fill: parent
            groups: ["A", "B"]
            source: "qrc:///particleresources/glowdot.png"
            colorVariation: 0
            color: "#00111111"
        }
        ImageParticle {
            id: flame
            anchors.fill: parent
            system: particles
            groups: ["C", "D"]
            source: "qrc:///particleresources/glowdot.png"
            colorVariation: 0.1
            color: "#00ff400f"
        }

        Emitter {
            id: fire
            system: particles
            group: "C"

            y: parent.height
            width: parent.width

            emitRate: 350
            lifeSpan: 3500

            acceleration: PointDirection { y: -17; xVariation: 3 }
            velocity: PointDirection {xVariation: 3}

            size: 24
            sizeVariation: 8
            endSize: 4
        }

        TrailEmitter {
            id: fireSmoke
            group: "B"
            system: particles
            follow: "C"
            width: root.width
            height: root.height - 68

            emitRatePerParticle: 1
            lifeSpan: 2000

            velocity: PointDirection {y:-17*6; yVariation: -17; xVariation: 3}
            acceleration: PointDirection {xVariation: 3}

            size: 36
            sizeVariation: 8
            endSize: 16
        }

        TrailEmitter {
            id: fireballFlame
            anchors.fill: parent
            system: particles
            group: "D"
            follow: "E"

            emitRatePerParticle: 120
            lifeSpan: 180
            emitWidth: TrailEmitter.ParticleSize
            emitHeight: TrailEmitter.ParticleSize
            emitShape: EllipseShape{}

            size: 16
            sizeVariation: 4
            endSize: 4
        }

        TrailEmitter {
            id: fireballSmoke
            anchors.fill: parent
            system: particles
            group: "A"
            follow: "E"

            emitRatePerParticle: 128
            lifeSpan: 2400
            emitWidth: TrailEmitter.ParticleSize
            emitHeight: TrailEmitter.ParticleSize
            emitShape: EllipseShape{}

            velocity: PointDirection {yVariation: 16; xVariation: 16}
            acceleration: PointDirection {y: -16}

            size: 24
            sizeVariation: 8
            endSize: 8
        }

        Emitter {
            id: balls
            system: particles
            group: "E"

            y: parent.height
            width: parent.width

            emitRate: 2
            lifeSpan: 7000

            velocity: PointDirection {y:-17*4*2; xVariation: 6*6}
            acceleration: PointDirection {y: 17*2; xVariation: 6*6}

            size: 8
            sizeVariation: 4
        }

        Turbulence { //A bit of turbulence makes the smoke look better
            anchors.fill: parent
            groups: ["A","B"]
            strength: 32
            system: particles
        }
    }
}

