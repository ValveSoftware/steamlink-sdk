/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

// Use QtQuick 2.8 to get GraphicsInfo and the other new properties
import QtQuick 2.8

Item {
    Rectangle {
        color: "gray"
        anchors.margins: 10
        anchors.fill: parent
        Image {
            id: image1
            source: "qrc:/qt.png"
        }
        ShaderEffectSource {
            id: effectSource1
            sourceItem: image1
            hideSource: true
        }
        ShaderEffect { // wobble
            id: eff
            width: image1.width
            height: image1.height
            anchors.centerIn: parent

            property variant source: effectSource1
            property real amplitude: 0.04 * 0.2
            property real frequency: 20
            property real time: 0

            NumberAnimation on time { loops: Animation.Infinite; from: 0; to: Math.PI * 2; duration: 600 }

            property bool customVertexShader: false // the effect is fine with the default vs, but toggle this to test
            property bool useHLSLSourceString: false // toggle to provide HLSL shaders as strings instead of bytecode in files

            property string glslVertexShader:
                "uniform highp mat4 qt_Matrix;" +
                "attribute highp vec4 qt_Vertex;" +
                "attribute highp vec2 qt_MultiTexCoord0;" +
                "varying highp vec2 qt_TexCoord0;" +
                "void main() {" +
                "   qt_TexCoord0 = qt_MultiTexCoord0;" +
                "   gl_Position = qt_Matrix * qt_Vertex;" +
                "}"

            property string glslFragmentShader:
                "uniform sampler2D source;" +
                "uniform highp float amplitude;" +
                "uniform highp float frequency;" +
                "uniform highp float time;" +
                "uniform lowp float qt_Opacity;" +
                "varying highp vec2 qt_TexCoord0;" +
                "void main() {" +
                "    highp vec2 p = sin(time + frequency * qt_TexCoord0);" +
                "    gl_FragColor = texture2D(source, qt_TexCoord0 + amplitude * vec2(p.y, -p.x)) * qt_Opacity;" +
                "}"

            property string hlslVertexShader: "cbuffer ConstantBuffer : register(b0) {" +
                                              "    float4x4 qt_Matrix;" +
                                              "    float qt_Opacity; }" +
                                              "struct PSInput {" +
                                              "    float4 position : SV_POSITION;" +
                                              "    float2 coord : TEXCOORD0; };" +
                                              "PSInput main(float4 position : POSITION, float2 coord : TEXCOORD0) {" +
                                              "    PSInput result;" +
                                              "    result.position = mul(qt_Matrix, position);" +
                                              "    result.coord = coord;" +
                                              "    return result;" +
                                              "}";

            property string hlslPixelShader:"cbuffer ConstantBuffer : register(b0) {" +
                                            "    float4x4 qt_Matrix;" +
                                            "    float qt_Opacity;" +
                                            "    float amplitude;" +
                                            "    float frequency;" +
                                            "    float time; }" +
                                            "Texture2D source : register(t0);" +
                                            "SamplerState sourceSampler : register(s0);" +
                                            "float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET" +
                                            "{" +
                                            "    float2 p = sin(time + frequency * coord);" +
                                            "    return source.Sample(sourceSampler, coord + amplitude * float2(p.y, -p.x)) * qt_Opacity;" +
                                            "}";

            property string hlslVertexShaderByteCode: "qrc:/vs_wobble.cso"
            property string hlslPixelShaderByteCode: "qrc:/ps_wobble.cso"

            vertexShader: customVertexShader ? (GraphicsInfo.shaderType === GraphicsInfo.HLSL
                                                ? (useHLSLSourceString ? hlslVertexShader : hlslVertexShaderByteCode)
                                                : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? glslVertexShader : "")) : ""

            fragmentShader: GraphicsInfo.shaderType === GraphicsInfo.HLSL
                            ? (useHLSLSourceString ? hlslPixelShader : hlslPixelShaderByteCode)
                            : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? glslFragmentShader : "")
        }

        Image {
            id: image2
            source: "qrc:/face-smile.png"
        }
        ShaderEffectSource {
            id: effectSource2
            sourceItem: image2
            hideSource: true
        }
        ShaderEffect { // dropshadow
            id: eff2
            width: image2.width
            height: image2.height
            scale: 2
            x: 40
            y: 40

            property variant source: effectSource2

            property string glslShaderPass1: "
                uniform lowp float qt_Opacity;
                uniform sampler2D source;
                uniform highp vec2 delta;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_FragColor = (0.0538 * texture2D(source, qt_TexCoord0 - 3.182 * delta)
                        + 0.3229 * texture2D(source, qt_TexCoord0 - 1.364 * delta)
                        + 0.2466 * texture2D(source, qt_TexCoord0)
                        + 0.3229 * texture2D(source, qt_TexCoord0 + 1.364 * delta)
                        + 0.0538 * texture2D(source, qt_TexCoord0 + 3.182 * delta)) * qt_Opacity;
                }"
            property string glslShaderPass2: "
                uniform lowp float qt_Opacity;
                uniform highp vec2 offset;
                uniform sampler2D source;
                uniform sampler2D shadow;
                uniform highp float darkness;
                uniform highp vec2 delta;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    lowp vec4 fg = texture2D(source, qt_TexCoord0);
                    lowp vec4 bg = texture2D(shadow, qt_TexCoord0 + delta);
                    gl_FragColor = (fg + vec4(0., 0., 0., darkness * bg.a) * (1. - fg.a)) * qt_Opacity;
                }"

            property variant shadow: ShaderEffectSource {
                sourceItem: ShaderEffect {
                    width: eff2.width
                    height: eff2.height
                    property variant delta: Qt.size(0.0, 1.0 / height)
                    property variant source: ShaderEffectSource {
                        sourceItem: ShaderEffect {
                            id: innerEff
                            width: eff2.width
                            height: eff2.height
                            property variant delta: Qt.size(1.0 / width, 0.0)
                            property variant source: effectSource2
                            fragmentShader: GraphicsInfo.shaderType === GraphicsInfo.HLSL ? "qrc:/ps_shadow1.cso" : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? eff2.glslShaderPass1 : "")
                        }
                    }
                    fragmentShader: GraphicsInfo.shaderType === GraphicsInfo.HLSL ? "qrc:/ps_shadow1.cso" : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? eff2.glslShaderPass1: "")
                }
            }
            property real angle: 0
            property variant offset: Qt.point(5.0 * Math.cos(angle), 5.0 * Math.sin(angle))
            NumberAnimation on angle { loops: Animation.Infinite; from: 0; to: Math.PI * 2; duration: 6000 }
            property variant delta: Qt.size(offset.x / width, offset.y / height)
            property real darkness: 0.5
            fragmentShader: GraphicsInfo.shaderType === GraphicsInfo.HLSL ? "qrc:/ps_shadow2.cso" : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? glslShaderPass2 : "")
        }

        Column {
            anchors.bottom: parent.bottom
            Text {
                color: "yellow"
                font.pointSize: 24
                text: "Shader effect is " + (GraphicsInfo.shaderType === GraphicsInfo.HLSL ? "HLSL" : (GraphicsInfo.shaderType === GraphicsInfo.GLSL ? "GLSL" : "UNKNOWN")) + " based";
            }
            Text {
                text: GraphicsInfo.shaderType + " " + GraphicsInfo.shaderCompilationType + " " + GraphicsInfo.shaderSourceType
            }
            Text {
                text: eff.status + " " + eff.log
            }
        }
    }
}
