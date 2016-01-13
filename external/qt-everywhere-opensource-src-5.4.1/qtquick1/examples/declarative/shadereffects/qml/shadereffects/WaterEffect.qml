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

import QtQuick 1.0
import Qt.labs.shaders 1.0

Item {
    id: root
    property alias sourceItem: effectsource.sourceItem
    property real intensity: 1
    property bool waving: true
    anchors.top: sourceItem.bottom
    width: sourceItem.width
    height: sourceItem.height

    ShaderEffectItem {
        anchors.fill: parent
        property variant source: effectsource
        property real f: 0
        property real f2: 0
        property alias intensity: root.intensity
        smooth: true

        ShaderEffectSource {
            id: effectsource
            hideSource: false
            smooth: true
        }

        fragmentShader:
            "
            varying highp vec2 qt_TexCoord0;
            uniform sampler2D source;
            uniform lowp float qt_Opacity;
            uniform highp float f;
            uniform highp float f2;
            uniform highp float intensity;

            void main() {
                const highp float twopi = 3.141592653589 * 2.0;

                highp float distanceFactorToPhase = pow(qt_TexCoord0.y + 0.5, 8.0) * 5.0;
                highp float ofx = sin(f * twopi + distanceFactorToPhase) / 100.0;
                highp float ofy = sin(f2 * twopi + distanceFactorToPhase * qt_TexCoord0.x) / 60.0;

                highp float intensityDampingFactor = (qt_TexCoord0.x + 0.1) * (qt_TexCoord0.y + 0.2);
                highp float distanceFactor = (1.0 - qt_TexCoord0.y) * 4.0 * intensity * intensityDampingFactor;

                ofx *= distanceFactor;
                ofy *= distanceFactor;

                highp float x = qt_TexCoord0.x + ofx;
                highp float y = 1.0 - qt_TexCoord0.y + ofy;

                highp float fake = (sin((ofy + ofx) * twopi) + 0.5) * 0.05 * (1.2 - qt_TexCoord0.y) * intensity * intensityDampingFactor;

                highp vec4 pix =
                    texture2D(source, vec2(x, y)) * 0.6 +
                    texture2D(source, vec2(x-fake, y)) * 0.15 +
                    texture2D(source, vec2(x, y-fake)) * 0.15 +
                    texture2D(source, vec2(x+fake, y)) * 0.15 +
                    texture2D(source, vec2(x, y+fake)) * 0.15;

                highp float darken = 0.6 - (ofx - ofy) / 2.0;
                pix.b *= 1.2 * darken;
                pix.r *= 0.9 * darken;
                pix.g *= darken;

                gl_FragColor = qt_Opacity * vec4(pix.r, pix.g, pix.b, 1.0);
            }
            "

        NumberAnimation on f {
            running: root.waving
            loops: Animation.Infinite
            from: 0
            to: 1
            duration: 2410
        }
        NumberAnimation on f2 {
            running: root.waving
            loops: Animation.Infinite
            from: 0
            to: 1
            duration: 1754
        }
    }
}
