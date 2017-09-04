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
    width: 360
    height: 600
    color: "black"
    ParticleSystem {
        anchors.fill: parent
        id: syssy
        //! [0]
        ParticleGroup {
            name: "fire"
            duration: 2000
            durationVariation: 2000
            to: {"splode":1}
        }
        //! [0]
        //! [1]
        ParticleGroup {
            name: "splode"
            duration: 400
            to: {"dead":1}
            TrailEmitter {
                group: "works"
                emitRatePerParticle: 100
                lifeSpan: 1000
                maximumEmitted: 1200
                size: 8
                velocity: AngleDirection {angle: 270; angleVariation: 45; magnitude: 20; magnitudeVariation: 20;}
                acceleration: PointDirection {y:100; yVariation: 20}
            }
        }
        //! [1]
        //! [2]
        ParticleGroup {
            name: "dead"
            duration: 1000
            Affector {
                once: true
                onAffected: worksEmitter.burst(400,x,y)
            }
        }
        //! [2]

        Timer {
            interval: 6000
            running: true
            triggeredOnStart: true
            repeat: true
            onTriggered:startingEmitter.pulse(100);
        }
        Emitter {
            id: startingEmitter
            group: "fire"
            width: parent.width
            y: parent.height
            enabled: false
            emitRate: 80
            lifeSpan: 6000
            velocity: PointDirection {y:-100;}
            size: 32
        }

        Emitter {
            id: worksEmitter
            group: "works"
            enabled: false
            emitRate: 100
            lifeSpan: 1600
            maximumEmitted: 6400
            size: 8
            velocity: CumulativeDirection {
                PointDirection {y:-100}
                AngleDirection {angleVariation: 360; magnitudeVariation: 80;}
            }
            acceleration: PointDirection {y:100; yVariation: 20}
        }

        ImageParticle {
            groups: ["works", "fire", "splode"]
            source: "qrc:///particleresources/glowdot.png"
            entryEffect: ImageParticle.Scale
        }
    }
}

