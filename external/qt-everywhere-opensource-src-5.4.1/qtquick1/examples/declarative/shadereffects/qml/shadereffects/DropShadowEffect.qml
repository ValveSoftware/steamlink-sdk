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
    id: main
    property real blur: 0.0
    property alias color: shadowEffectWithHBlur.color
    property alias sourceItem: source.sourceItem

    ShaderEffectSource {
        id: source
        smooth: true
        hideSource: false
    }

    ShaderEffectItem {
        id: shadowEffectWithHBlur
        anchors.fill: parent

        property color color: "grey"
        property variant sourceTexture: source;
        property real xStep: main.blur / main.width

        vertexShader:"
            uniform highp mat4 qt_ModelViewProjectionMatrix;
            attribute highp vec4 qt_Vertex;
            attribute highp vec2 qt_MultiTexCoord0;
            uniform highp float xStep;
            varying highp vec2 qt_TexCoord0;
            varying highp vec2 qt_TexCoord1;
            varying highp vec2 qt_TexCoord2;
            varying highp vec2 qt_TexCoord4;
            varying highp vec2 qt_TexCoord5;
            varying highp vec2 qt_TexCoord6;

            void main(void)
            {
                highp vec2 shift = vec2(xStep, 0.);
                qt_TexCoord0 = qt_MultiTexCoord0 - 2.5 * shift;
                qt_TexCoord1 = qt_MultiTexCoord0 - 1.5 * shift;
                qt_TexCoord2 = qt_MultiTexCoord0 - 0.5 * shift;
                qt_TexCoord4 = qt_MultiTexCoord0 + 0.5 * shift;
                qt_TexCoord5 = qt_MultiTexCoord0 + 1.5 * shift;
                qt_TexCoord6 = qt_MultiTexCoord0 + 2.5 * shift;
                gl_Position =  qt_ModelViewProjectionMatrix * qt_Vertex;
            }
        "

        fragmentShader:"
            uniform highp vec4 color;
            uniform lowp sampler2D sourceTexture;
            varying highp vec2 qt_TexCoord0;
            varying highp vec2 qt_TexCoord1;
            varying highp vec2 qt_TexCoord2;
            varying highp vec2 qt_TexCoord4;
            varying highp vec2 qt_TexCoord5;
            varying highp vec2 qt_TexCoord6;

            void main() {
                highp vec4 sourceColor = (texture2D(sourceTexture, qt_TexCoord0) * 0.1
                + texture2D(sourceTexture, qt_TexCoord1) * 0.15
                + texture2D(sourceTexture, qt_TexCoord2) * 0.25
                + texture2D(sourceTexture, qt_TexCoord4) * 0.25
                + texture2D(sourceTexture, qt_TexCoord5) * 0.15
                + texture2D(sourceTexture, qt_TexCoord6) * 0.1);
                gl_FragColor = mix(vec4(0), color, sourceColor.a);
            }
        "
    }

    ShaderEffectSource {
        id: hBlurredShadow
        smooth: true
        sourceItem: shadowEffectWithHBlur
        hideSource: true
    }

    ShaderEffectItem {
        id: finalEffect
        anchors.fill: parent

        property color color: "grey"
        property variant sourceTexture: hBlurredShadow;
        property real yStep: main.blur / main.height

        vertexShader:"
            uniform highp mat4 qt_ModelViewProjectionMatrix;
            attribute highp vec4 qt_Vertex;
            attribute highp vec2 qt_MultiTexCoord0;
            uniform highp float yStep;
            varying highp vec2 qt_TexCoord0;
            varying highp vec2 qt_TexCoord1;
            varying highp vec2 qt_TexCoord2;
            varying highp vec2 qt_TexCoord4;
            varying highp vec2 qt_TexCoord5;
            varying highp vec2 qt_TexCoord6;

            void main(void)
            {
                highp vec2 shift = vec2(0., yStep);
                qt_TexCoord0 = qt_MultiTexCoord0 - 2.5 * shift;
                qt_TexCoord1 = qt_MultiTexCoord0 - 1.5 * shift;
                qt_TexCoord2 = qt_MultiTexCoord0 - 0.5 * shift;
                qt_TexCoord4 = qt_MultiTexCoord0 + 0.5 * shift;
                qt_TexCoord5 = qt_MultiTexCoord0 + 1.5 * shift;
                qt_TexCoord6 = qt_MultiTexCoord0 + 2.5 * shift;
                gl_Position =  qt_ModelViewProjectionMatrix * qt_Vertex;
            }
        "

        fragmentShader:"
            uniform highp vec4 color;
            uniform lowp sampler2D sourceTexture;
            uniform highp float qt_Opacity;
            varying highp vec2 qt_TexCoord0;
            varying highp vec2 qt_TexCoord1;
            varying highp vec2 qt_TexCoord2;
            varying highp vec2 qt_TexCoord4;
            varying highp vec2 qt_TexCoord5;
            varying highp vec2 qt_TexCoord6;

            void main() {
                highp vec4 sourceColor = (texture2D(sourceTexture, qt_TexCoord0) * 0.1
                + texture2D(sourceTexture, qt_TexCoord1) * 0.15
                + texture2D(sourceTexture, qt_TexCoord2) * 0.25
                + texture2D(sourceTexture, qt_TexCoord4) * 0.25
                + texture2D(sourceTexture, qt_TexCoord5) * 0.15
                + texture2D(sourceTexture, qt_TexCoord6) * 0.1);
                gl_FragColor = sourceColor * qt_Opacity;
            }
        "
    }
}
