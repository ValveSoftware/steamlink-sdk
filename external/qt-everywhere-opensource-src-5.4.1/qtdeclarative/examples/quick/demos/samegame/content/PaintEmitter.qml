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
import "."

Emitter {
    property Item block: parent
    anchors.fill: parent
    shape: EllipseShape { fill: true }
    group: {
        if (block.type == 0){
            "redspots";
        } else if (block.type == 1) {
            "bluespots";
        } else if (block.type == 2) {
            "greenspots";
        } else {
            "yellowspots";
        }
    }
    size: Settings.blockSize * 2
    endSize: Settings.blockSize/2
    lifeSpan: 30000
    enabled: false
    emitRate: 60
    maximumEmitted: 60
    velocity: PointDirection{ y: 4; yVariation: 4 }
    /* Possibly better, but dependent on gerrit change,28212
    property real mainIntensity: 0.8
    property real subIntensity: 0.1
    property real colorVariation: 0.005
    onEmitParticles: {//One group, many colors, for better stacking
        for (var i=0; i<particles.length; i++) {
            var particle = particles[i];
            if (block.type == 0) {
                particle.red = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.green = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.blue = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
            } else if (block.type == 1) {
                particle.red = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.green = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.blue = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
            } else if (block.type == 2) {
                particle.red = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.green = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.blue = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
            } else if (block.type == 3) {
                particle.red = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.green = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.blue = subIntensity + (Math.random() * colorVariation * 2 - colorVariation);
            } else {
                particle.red = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.green = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
                particle.blue = mainIntensity + (Math.random() * colorVariation * 2 - colorVariation);
            }
        }
    }
    */
}
