/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0

Entity {
    property string baseName: "";
    property string extension: ".png"

    property TextureCubeMap skyboxTexture: TextureCubeMap {
        generateMipMaps: false
        magnificationFilter: Texture.Linear
        minificationFilter: Texture.Linear
        wrapMode {
            x: WrapMode.ClampToEdge
            y: WrapMode.ClampToEdge
        }
        TextureImage { mirrored: false; face: Texture.CubeMapPositiveX; source: baseName + "_posx" + extension }
        TextureImage { mirrored: false; face: Texture.CubeMapPositiveY; source: baseName + "_posy" + extension }
        TextureImage { mirrored: false; face: Texture.CubeMapPositiveZ; source: baseName + "_posz" + extension }
        TextureImage { mirrored: false; face: Texture.CubeMapNegativeX; source: baseName + "_negx" + extension }
        TextureImage { mirrored: false; face: Texture.CubeMapNegativeY; source: baseName + "_negy" + extension }
        TextureImage { mirrored: false; face: Texture.CubeMapNegativeZ; source: baseName + "_negz" + extension }
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

    Material {
        id: skyboxMaterial
        parameters: Parameter { name: "skyboxTexture"; value: skyboxTexture}

        effect: Effect {
            FilterKey {
                id: forward
                name: "renderingStyle"
                value: "forward"
            }

            techniques: [
                // GL3 Technique
                Technique {
                    filterKeys: [ forward ]
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
                    filterKeys: [ forward ]
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
                    filterKeys: [ forward ]
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

    components: [cuboidMesh, skyboxMaterial]
}

