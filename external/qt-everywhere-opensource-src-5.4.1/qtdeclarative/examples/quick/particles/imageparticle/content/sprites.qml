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
    color: "lightsteelblue"
    width: 800
    height: 800
    id: root

    SpriteSequence {
        sprites: Sprite {
                name: "bear"
                source: "../../images/bear_tiles.png"
                frameCount: 13
                frameDuration: 120
            }
        width: 250
        height: 250
        x: 20
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        z:4
    }

    ParticleSystem { id: sys }

    ImageParticle {
        anchors.fill: parent
        id: particles
        system: sys
        sprites: [Sprite {
            name: "happy"
            source: "../../images/starfish_1.png"
            frameCount: 1
            frameDuration: 260
            to: {"happy": 1, "silly": 1, "angry": 1}
        }, Sprite {
            name: "angry"
            source: "../../images/starfish_0.png"
            frameCount: 1
            frameDuration: 260
            to: {"happy": 1, "silly": 1, "angry": 1}
        }, Sprite {
            name: "silly"
            source: "../../images/starfish_2.png"
            frameCount: 1
            frameDuration: 260
            to: {"happy": 1, "silly": 1, "noticedbear": 0}
        }, Sprite {
            name: "noticedbear"
            source: "../../images/starfish_3.png"
            frameCount: 1
            frameDuration: 2600
        }]
    }

    Emitter {
        system: sys
        emitRate: 2
        lifeSpan: 10000
        velocity: AngleDirection {angle: 90; magnitude: 60; angleVariation: 5}
        acceleration: PointDirection { y: 10 }
        size: 160
        sizeVariation: 40
        width: parent.width
        height: 100
    }

    SpriteGoal {
        system: sys
        width: root.width;
        height: root.height/2;
        y: root.height/2;
        goalState:"noticedbear"
    }
}
