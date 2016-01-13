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
    color: "goldenrod"
    width: 400
    height: 400
    ParticleSystem {id:sys}

    //! [spin]
    ImageParticle {
        system: sys
        groups: ["goingLeft", "goingRight"]
        source: "../../images/starfish_4.png"
        rotation: 90
        rotationVelocity: 90
        autoRotation: true
    }
    //! [spin]
    //! [deform]
    ImageParticle {
        system: sys
        groups: ["goingDown"]
        source: "../../images/starfish_0.png"
        rotation: 180
        yVector: PointDirection { y: 0.5; yVariation: 0.25; xVariation: 0.25; }
    }
    //! [deform]

    Timer {
        running: true
        repeat: false
        interval: 100
        onTriggered: emitA.enabled = true;
    }
    Timer {
        running: true
        repeat: false
        interval: 4200
        onTriggered: emitB.enabled = true;
    }
    Timer {
        running: true
        repeat: false
        interval: 8400
        onTriggered: emitC.enabled = true;
    }

    Emitter {
        id: emitA
        x: 0
        y: 120
        system: sys
        enabled: false
        group: "goingRight"
        velocity: PointDirection { x: 100 }
        lifeSpan: 4000
        emitRate: 1
        size: 128
    }
    Emitter {
        id: emitB
        x: 400
        y: 240
        system: sys
        enabled: false
        group: "goingLeft"
        velocity: PointDirection { x: -100 }
        lifeSpan: 4000
        emitRate: 1
        size: 128
    }
    Emitter {
        id: emitC
        x: 0
        y: 360
        system: sys
        enabled: false
        group: "goingDown"
        velocity: PointDirection { x: 100 }
        lifeSpan: 4000
        emitRate: 1
        size: 128
    }
}
