/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
    id: root

    property color ambient:  Qt.rgba(0.15, 0.35, 0.50, 1.0)
    property alias diffuse: diffuseTextureImage.source
    property alias normal: normalTextureImage.source
    property alias wave: waveTextureImage.source
    property alias specular: specularTextureImage.source
    property alias sky: skyTextureImage.source
    property alias foam: foamTextureImage.source

    property color specularColor: Qt.rgba(0.2, 0.2, 0.2, 1.0)
    property real shininess: 150.0
    property real textureScale: 1.0
    property real offsetx: 0.0
    property real offsety: 0.0
    property real wavescale: 0.0
    property real specularity: 1.0
    property real waveheight: 0.1
    property real waveStrenght: 0.1
    property real normalAmount: 2.0
    property real waveRandom: 1.0

    parameters: [
        Parameter { name: "ka"; value: Qt.vector3d(root.ambient.r, root.ambient.g, root.ambient.b) },
        Parameter {
            name: "foamTexture"
            value: Texture2D {
                id: foamTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true
                maximumAnisotropy: 16.0
                TextureImage { id: foamTextureImage }
            }
        },
        Parameter {
            name: "skyTexture"
            value: Texture2D {
                id: skyTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true
                maximumAnisotropy: 16.0
                TextureImage { id: skyTextureImage }
            }
        },
        Parameter {
            name: "waveTexture"
            value: Texture2D {
                id: waveTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true
                maximumAnisotropy: 16.0
                TextureImage { id: waveTextureImage }
            }
        },
        Parameter {
            name: "specularTexture"
            value: Texture2D {
                id: specularTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true
                maximumAnisotropy: 16.0
                TextureImage { id: specularTextureImage }
            }
        },
        Parameter {
            name: "diffuseTexture"
            value: Texture2D {
                id: diffuseTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true
                maximumAnisotropy: 16.0
                TextureImage { id: diffuseTextureImage }
            }
        },
        Parameter {
            name: "normalTexture"
            value: Texture2D {
                id: normalTexture
                minificationFilter: Texture.LinearMipMapLinear
                magnificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.Repeat
                    y: WrapMode.Repeat
                }
                generateMipMaps: true

                maximumAnisotropy: 16.0
                TextureImage { id: normalTextureImage }
            }
        },
        Parameter {
            name: "specularColor"
            value: Qt.vector3d(root.specularColor.r, root.specularColor.g, root.specularColor.b)
        },
        Parameter { name: "shininess"; value: root.shininess },
        Parameter { name: "texCoordScale"; value: textureScale },
        Parameter { name: "offsetx"; value: root.offsetx },
        Parameter { name: "offsety"; value: root.offsety },
        Parameter { name: "vertYpos"; value: root.wavescale },
        Parameter { name: "specularity"; value: root.specularity },
        Parameter { name: "waveheight"; value: root.waveheight },
        Parameter { name: "waveStrenght"; value: root.waveStrenght },
        Parameter { name: "waveRandom"; value: root.waveRandom },
        Parameter { name: "normalAmount"; value: root.normalAmount }
    ]


    effect: Effect {
        property string vertex: "qrc:/shaders/gl3/water.vert"
        property string fragment: "qrc:/shaders/gl3/water.frag"
        property string vertexES: "qrc:/shaders/es2/water.vert"
        property string fragmentES: "qrc:/shaders/es2/water.frag"

        FilterKey {
            id: forward
            name: "renderingStyle"
            value: "forward"
        }
        ShaderProgram {
            id: gl3Shader
            vertexShaderCode: loadSource(parent.vertex)
            fragmentShaderCode: loadSource(parent.fragment)
        }
        ShaderProgram {
            id: esShader
            vertexShaderCode: loadSource(parent.vertexES)
            fragmentShaderCode: loadSource(parent.fragmentES)
        }

        AlphaCoverage { id: alphaCoverage }

        DepthTest {
            id: depth
            depthFunction: DepthTest.Less }

        techniques: [
            // OpenGL 3.1
            Technique {
                filterKeys: [ forward ]
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGL
                    profile: GraphicsApiFilter.CoreProfile
                    majorVersion: 3
                    minorVersion: 1
                }
                renderPasses: RenderPass {
                    shaderProgram: gl3Shader
                    renderStates: [alphaCoverage ]
                }
            },

            // OpenGLES 2.0
            Technique {
                filterKeys: [ forward ]
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGLES
                    majorVersion: 2
                    minorVersion: 0
                }
                renderPasses: RenderPass {
                    shaderProgram: esShader
                    renderStates: [ alphaCoverage ]
                }
            },

            // OpenGL ES 2
            Technique {
                filterKeys: [ forward ]
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGLES
                    profile: GraphicsApiFilter.NoProfile
                    majorVersion: 2
                    minorVersion: 0
                }
                renderPasses: RenderPass {
                    shaderProgram: esShader
                    renderStates: [ alphaCoverage ]
                }
            }
        ]
    }
}


