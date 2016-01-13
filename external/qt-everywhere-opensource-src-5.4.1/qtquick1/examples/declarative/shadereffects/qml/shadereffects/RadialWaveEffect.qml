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

ShaderEffectItem {
    id: effect

    property real wave: 0.3
    property real waveOriginX: 0.5
    property real waveOriginY: 0.5
    property real waveWidth: 0.01
    property real aspectRatio: width/height
    property variant source: 0

    fragmentShader:
        "
        varying mediump vec2 qt_TexCoord0;
        uniform sampler2D source;
        uniform highp float wave;
        uniform highp float waveWidth;
        uniform highp float waveOriginX;
        uniform highp float waveOriginY;
        uniform highp float aspectRatio;

        void main(void)
        {
        mediump vec2 texCoord2 = qt_TexCoord0;
        mediump vec2 origin = vec2(waveOriginX, (1.0 - waveOriginY) / aspectRatio);

        highp float fragmentDistance = distance(vec2(texCoord2.s, texCoord2.t / aspectRatio), origin);
        highp float waveLength = waveWidth + fragmentDistance * 0.25;

        if ( fragmentDistance > wave && fragmentDistance < wave + waveLength) {
            highp float distanceFromWaveEdge = min(abs(wave - fragmentDistance), abs(wave + waveLength - fragmentDistance));
            texCoord2 += sin(1.57075 * distanceFromWaveEdge / waveLength) * distanceFromWaveEdge * 0.08 / fragmentDistance;
        }

        gl_FragColor = texture2D(source, texCoord2.st);
        }
        "
}
