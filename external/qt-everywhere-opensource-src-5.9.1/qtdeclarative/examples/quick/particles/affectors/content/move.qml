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
    height: 540
    color: "black"
    ParticleSystem {
        anchors.fill: parent
        ImageParticle {
            groups: ["A"]
            anchors.fill: parent
            source: "qrc:///particleresources/star.png"
            color:"#FF1010"
            redVariation: 0.8
        }

        Emitter {
            group: "A"
            emitRate: 100
            lifeSpan: 2800
            size: 32
            sizeVariation: 8
            velocity: PointDirection{ x: 66; xVariation: 20 }
            width: 80
            height: 80
        }

        //! [A]
        Affector {
            groups: ["A"]
            x: 120
            width: 80
            height: 80
            once: true
            position: PointDirection { x: 120; }
        }
        //! [A]

        ImageParticle {
            groups: ["B"]
            anchors.fill: parent
            source: "qrc:///particleresources/star.png"
            color:"#10FF10"
            greenVariation: 0.8
        }

        Emitter {
            group: "B"
            emitRate: 100
            lifeSpan: 2800
            size: 32
            sizeVariation: 8
            velocity: PointDirection{ x: 240; xVariation: 60 }
            y: 260
            width: 10
            height: 10
        }

        //! [B]
        Affector {
            groups: ["B"]
            x: 120
            y: 240
            width: 80
            height: 80
            once: true
            velocity: AngleDirection { angleVariation:360; magnitude: 72 }
        }
        //! [B]

        ImageParticle {
            groups: ["C"]
            anchors.fill: parent
            source: "qrc:///particleresources/star.png"
            color:"#1010FF"
            blueVariation: 0.8
        }

        Emitter {
            group: "C"
            y: 400
            emitRate: 100
            lifeSpan: 2800
            size: 32
            sizeVariation: 8
            velocity: PointDirection{ x: 80; xVariation: 10 }
            acceleration: PointDirection { y: 10; x: 20; }
            width: 80
            height: 80
        }

        //! [C]
        Affector {
            groups: ["C"]
            x: 120
            y: 400
            width: 80
            height: 120
            once: true
            relative: false
            acceleration: PointDirection { y: -80; }
        }
        //! [C]

    }
}
