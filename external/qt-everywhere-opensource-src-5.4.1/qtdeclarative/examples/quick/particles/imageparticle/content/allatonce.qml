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
    color: "white"
    width: 640
    height: 480
    ParticleSystem {
        id: sys
    }

    ImageParticle {
        // ![0]
        sprites: [
            Sprite {
                name: "bear"
                source: "../../images/bear_tiles.png"
                frameCount: 13
                frameDuration: 120
            }
        ]
        colorVariation: 0.5
        rotationVelocityVariation: 360
        colorTable: "../../images/colortable.png"
        // ![0]
        system: sys
    }

    Friction {
        factor: 0.1
        system: sys
    }

    Emitter {
        system: sys
        anchors.centerIn: parent
        id: particles
        emitRate: 200
        lifeSpan: 6000
        velocity: AngleDirection {angleVariation: 360; magnitude: 80; magnitudeVariation: 40}
        size: 60
        endSize: 120
    }

    Text {
        x: 16
        y: 16
        text: "QML..."
        style: Text.Outline; styleColor: "#AAAAAA"
        font.pixelSize: 32
    }
    Text {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 16
        text: "... can you be trusted with the power?"
        style: Text.Outline; styleColor: "#AAAAAA"
        font.pixelSize: width > 400 ? 32 : 16
    }
}
