/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
import Qt3D.Extras 2.0

Entity {

    property alias cameraPosition: transform.translation;
    property string sourceDirectory: "";
    property string extension: ".webp"

    property TextureCubeMap skyboxTexture: TextureCubeMap {
        generateMipMaps: false
        magnificationFilter: Texture.Linear
        minificationFilter: Texture.Linear
        wrapMode {
            x: WrapMode.ClampToEdge
            y: WrapMode.ClampToEdge
        }
        TextureImage { face: Texture.CubeMapPositiveX; mirrored: false; source: sourceDirectory + "_posx" + extension }
        TextureImage { face: Texture.CubeMapPositiveY; mirrored: false; source: sourceDirectory + "_posy" + extension }
        TextureImage { face: Texture.CubeMapPositiveZ; mirrored: false; source: sourceDirectory + "_posz" + extension }
        TextureImage { face: Texture.CubeMapNegativeX; mirrored: false; source: sourceDirectory + "_negx" + extension }
        TextureImage { face: Texture.CubeMapNegativeY; mirrored: false; source: sourceDirectory + "_negy" + extension }
        TextureImage { face: Texture.CubeMapNegativeZ; mirrored: false; source: sourceDirectory + "_negz" + extension }
    }

    ShaderProgram {
        id: gl3SkyboxShader
        vertexShaderCode: loadSource("qrc:/shaders/gl3/skybox.vert")
        fragmentShaderCode: loadSource("qrc:/shaders/gl3/skybox.frag")
    }

    ShaderProgram {
        id: gl2es2SkyboxShader
        vertexShaderCode: loadSource("qrc:/shaders/es2/skybox.vert")
        fragmentShaderCode: loadSource("qrc:/shaders/es2/skybox.frag")
    }

    CuboidMesh {
        id: cuboidMesh
        yzMeshResolution: Qt.size(2, 2)
        xzMeshResolution: Qt.size(2, 2)
        xyMeshResolution: Qt.size(2, 2)
    }

    Transform {
        id: transform
    }

    Material {
        id: skyboxMaterial
        parameters: Parameter { name: "skyboxTexture"; value: skyboxTexture}

        effect: Effect {
            techniques: [
                // GL3 Technique
                Technique {
                    graphicsApiFilter {
                        api: GraphicsApiFilter.OpenGL
                        profile: GraphicsApiFilter.CoreProfile
                        majorVersion: 3
                        minorVersion: 1
                    }
                    renderPasses: RenderPass {
                        shaderProgram: gl3SkyboxShader
                        renderStates: [
                            // cull front faces
                            CullFace { mode: CullFace.Front },
                            DepthTest { depthFunction: DepthTest.LessOrEqual }
                        ]
                    }
                },
                Technique {
                    graphicsApiFilter {
                        api: GraphicsApiFilter.OpenGL
                        profile: GraphicsApiFilter.NoProfile
                        majorVersion: 2
                        minorVersion: 0
                    }
                    renderPasses: RenderPass {
                        shaderProgram: gl2es2SkyboxShader
                        renderStates: [
                            CullFace { mode: CullFace.Front },
                            DepthTest { depthFunction: DepthTest.LessOrEqual }
                        ]
                    }
                },
                Technique {
                    graphicsApiFilter {
                        api: GraphicsApiFilter.OpenGLES
                        profile: GraphicsApiFilter.NoProfile
                        majorVersion: 2
                        minorVersion: 0
                    }
                    renderPasses: RenderPass {
                        shaderProgram: gl2es2SkyboxShader
                        renderStates: [
                            CullFace { mode: CullFace.Front },
                            DepthTest { depthFunction: DepthTest.LessOrEqual }
                        ]
                    }
                }
            ]
        }
    }

    components: [cuboidMesh, skyboxMaterial, transform]
}

