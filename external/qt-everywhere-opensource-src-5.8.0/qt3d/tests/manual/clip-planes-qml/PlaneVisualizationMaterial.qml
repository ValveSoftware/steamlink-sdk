/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import Qt3D.Core 2.0
import Qt3D.Render 2.0

Material {
    id:root
    property color ambient:  Qt.rgba( 0.05, 0.05, 0.05, 1.0 )
    property color diffuse:  Qt.rgba( 0.7, 0.7, 0.7, 1.0 )
    property color specular: Qt.rgba( 0.95, 0.95, 0.95, 1.0 )
    property real shininess: 150.0
    property real alpha: 0.5


    ShaderProgram {
        id: gl3PhongAlphaShader
        vertexShaderCode: loadSource("qrc:/shaders/gl3/phong.vert")
        fragmentShaderCode: loadSource("qrc:/shaders/gl3/phongalpha.frag")
    }

    ShaderProgram {
        id: gl2es2PhongAlphaShader
        vertexShaderCode: loadSource("qrc:/shaders/es2/phong.vert")
        fragmentShaderCode: loadSource("qrc:/shaders/es2/phongalpha.frag")
    }

    effect: Effect {

        parameters: [
            Parameter { name: "alpha";  value: root.alpha },
            Parameter { name: "ka";   value: Qt.vector3d(root.ambient.r, root.ambient.g, root.ambient.b) },
            Parameter { name: "kd";   value: Qt.vector3d(root.diffuse.r, root.diffuse.g, root.diffuse.b) },
            Parameter { name: "ks";  value: Qt.vector3d(root.specular.r, root.specular.g, root.specular.b) },
            Parameter { name: "shininess"; value: root.shininess },
            Parameter { name: "lightPosition"; value: Qt.vector4d(1.0, 1.0, 0.0, 1.0) },
            Parameter { name: "lightIntensity"; value: Qt.vector3d(1.0, 1.0, 1.0) }
        ]

        techniques: [
            // GL 3 Technique
            Technique {
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGL
                    profile: GraphicsApiFilter.CoreProfile
                    majorVersion: 3
                    minorVersion: 1
                }
                renderPasses: RenderPass {
                    shaderProgram: gl3PhongAlphaShader
                    renderStates: [
                        NoDepthMask { },
                        BlendEquationArguments {
                            sourceRgb: BlendEquationArguments.SourceAlpha
                            destinationRgb: BlendEquationArguments.OneMinusSourceAlpha
                        },
                        BlendEquation {blendFunction: BlendEquation.Add}
                    ]
                    filterKeys: FilterKey { name: "pass"; value: "material" }
                }
            },

            // GL 2 Technique
            Technique {
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGL
                    profile: GraphicsApiFilter.NoProfile
                    majorVersion: 2
                    minorVersion: 0
                }
                renderPasses: RenderPass {
                    shaderProgram: gl2es2PhongAlphaShader
                    renderStates: [
                        NoDepthMask { },
                        BlendEquationArguments {
                            sourceRgb: BlendEquationArguments.SourceAlpha
                            destinationRgb: BlendEquationArguments.OneMinusSourceAlpha
                        },
                        BlendEquation {blendFunction: BlendEquation.Add}
                    ]
                    filterKeys: FilterKey { name: "pass"; value: "material" }
                }
            }
        ]
    }
}

