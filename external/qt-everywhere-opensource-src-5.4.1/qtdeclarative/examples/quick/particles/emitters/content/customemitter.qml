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

ParticleSystem {
    id: sys
    width: 360
    height: 600
    running: true
    Rectangle {
        z: -1
        anchors.fill: parent
        color: "black"
    }

    property real petalLength: 180
    property real petalRotation: 0
    NumberAnimation on petalRotation {
        from: 0;
        to: 360;
        loops: -1;
        running: true
        duration: 24000
    }

    function convert(a) {return a*(Math.PI/180);}
    Emitter {
        lifeSpan: 4000
        emitRate: 120
        size: 12
        anchors.centerIn: parent
        //! [0]
        onEmitParticles: {
            for (var i=0; i<particles.length; i++) {
                var particle = particles[i];
                particle.startSize = Math.max(02,Math.min(492,Math.tan(particle.t/2)*24));
                var theta = Math.floor(Math.random() * 6.0);
                particle.red = theta == 0 || theta == 1 || theta == 2 ? 0.2 : 1;
                particle.green = theta == 2 || theta == 3 || theta == 4 ? 0.2 : 1;
                particle.blue = theta == 4 || theta == 5 || theta == 0 ? 0.2 : 1;
                theta /= 6.0;
                theta *= 2.0*Math.PI;
                theta += sys.convert(sys.petalRotation);//Convert from degrees to radians
                particle.initialVX = petalLength * Math.cos(theta);
                particle.initialVY = petalLength * Math.sin(theta);
                particle.initialAX = particle.initialVX * -0.5;
                particle.initialAY = particle.initialVY * -0.5;
            }
        }
        //! [0]
    }

    ImageParticle {
        source: "qrc:///particleresources/fuzzydot.png"
        alpha: 0.0
    }
}
