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
    width: 400
    height: 400
    Rectangle {
        id: root
        color: "white"
        width: 310
        height: 300
        anchors.centerIn: parent
        ParticleSystem { id: sys }
        CustomParticle {
            system: sys
            property real maxWidth: root.width
            property real maxHeight: root.height
            ShaderEffectSource {
                id: pictureSource
                sourceItem: picture
                hideSource: true
            }
            Image {
                id: picture
                source: "../../images/starfish_3.png"
            }
            ShaderEffectSource {
                id: particleSource
                sourceItem: particle
                hideSource: true
            }
            Image {
                id: particle
                source: "qrc:///particleresources/fuzzydot.png"
            }
            //! [vertex]
            vertexShader:"
                uniform highp float maxWidth;
                uniform highp float maxHeight;
                varying highp vec2 fTex2;
                varying lowp float fFade;
                uniform lowp float qt_Opacity;

                void main() {

                    fTex2 = vec2(qt_ParticlePos.x, qt_ParticlePos.y);
                    //Uncomment this next line for each particle to use full texture, instead of the solid color at the center of the particle.
                    //fTex2 = fTex2 + ((- qt_ParticleData.z / 2. + qt_ParticleData.z) * qt_ParticleTex); //Adjusts size so it's like a chunk of image.
                    fTex2 = fTex2 / vec2(maxWidth, maxHeight);
                    highp float t = (qt_Timestamp - qt_ParticleData.x) / qt_ParticleData.y;
                    fFade = min(t*4., (1.-t*t)*.75) * qt_Opacity;
                    defaultMain();
                }
            "
            //! [vertex]
            property variant particleTexture: particleSource
            property variant pictureTexture: pictureSource
            //! [fragment]
            fragmentShader: "
                uniform sampler2D particleTexture;
                uniform sampler2D pictureTexture;
                varying highp vec2 qt_TexCoord0;
                varying highp vec2 fTex2;
                varying lowp float fFade;
                void main() {
                    gl_FragColor = texture2D(pictureTexture, fTex2) * texture2D(particleTexture, qt_TexCoord0).w * fFade;
            }"
            //! [fragment]
        }

        Emitter {
            id: emitter
            system: sys
            enabled: false
            lifeSpan: 8000
            maximumEmitted: 4000
            anchors.fill: parent
            size: 16
            acceleration: PointDirection { xVariation: 12; yVariation: 12 }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: emitter.burst(4000);
        }
    }
}
