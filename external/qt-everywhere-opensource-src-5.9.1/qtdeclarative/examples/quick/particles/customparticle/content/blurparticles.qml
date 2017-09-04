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
    color: "white"
    width: 240
    height: 360
    ParticleSystem {
        id: sys
    }
    Emitter {
        system:sys
        height: parent.height
        emitRate: 1
        lifeSpan: 12000
        velocity: PointDirection {x:20;}
        size: 128
    }
    ShaderEffectSource {
        id: theSource
        sourceItem: theItem
        hideSource: true
    }
    Image {
        id: theItem
        source: "../../images/starfish_1.png"
    }

    CustomParticle {
        system: sys
        //! [vertex]
        vertexShader:"
            uniform lowp float qt_Opacity;
            varying lowp float fFade;
            varying lowp float fBlur;

            void main() {
                defaultMain();
                highp float t = (qt_Timestamp - qt_ParticleData.x) / qt_ParticleData.y;
                highp float fadeIn = min(t * 10., 1.);
                highp float fadeOut = 1. - max(0., min((t - 0.75) * 4., 1.));

                fFade = fadeIn * fadeOut * qt_Opacity;
                fBlur = max(0.2 * t, t * qt_ParticleR);
            }
        "
        //! [vertex]
        property variant source: theSource
        property variant blurred: ShaderEffectSource {
        sourceItem: ShaderEffect {
            width: theItem.width
            height: theItem.height
            property variant delta: Qt.size(0.0, 1.0 / height)
            property variant source: ShaderEffectSource {
                sourceItem: ShaderEffect {
                    width: theItem.width
                    height: theItem.height
                    property variant delta: Qt.size(1.0 / width, 0.0)
                    property variant source: theSource
                    fragmentShader: "
                        uniform sampler2D source;
                        uniform lowp float qt_Opacity;
                        uniform highp vec2 delta;
                        varying highp vec2 qt_TexCoord0;
                        void main() {
                            gl_FragColor =(0.0538 * texture2D(source, qt_TexCoord0 - 3.182 * delta)
                                         + 0.3229 * texture2D(source, qt_TexCoord0 - 1.364 * delta)
                                         + 0.2466 * texture2D(source, qt_TexCoord0)
                                         + 0.3229 * texture2D(source, qt_TexCoord0 + 1.364 * delta)
                                         + 0.0538 * texture2D(source, qt_TexCoord0 + 3.182 * delta)) * qt_Opacity;
                        }"
                }
            }
            fragmentShader: "
                uniform sampler2D source;
                uniform lowp float qt_Opacity;
                uniform highp vec2 delta;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_FragColor =(0.0538 * texture2D(source, qt_TexCoord0 - 3.182 * delta)
                                 + 0.3229 * texture2D(source, qt_TexCoord0 - 1.364 * delta)
                                 + 0.2466 * texture2D(source, qt_TexCoord0)
                                 + 0.3229 * texture2D(source, qt_TexCoord0 + 1.364 * delta)
                                 + 0.0538 * texture2D(source, qt_TexCoord0 + 3.182 * delta)) * qt_Opacity;
                }"
            }
        }
        //! [fragment]
        fragmentShader: "
            uniform sampler2D source;
            uniform sampler2D blurred;
            varying highp vec2 qt_TexCoord0;
            varying highp float fBlur;
            varying highp float fFade;
            void main() {
                gl_FragColor = mix(texture2D(source, qt_TexCoord0), texture2D(blurred, qt_TexCoord0), min(1.0,fBlur*3.0)) * fFade;
            }"
        //! [fragment]

    }
}

