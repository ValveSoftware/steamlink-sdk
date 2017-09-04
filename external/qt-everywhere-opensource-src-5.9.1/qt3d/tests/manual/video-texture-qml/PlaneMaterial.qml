/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import Qt3D.Core 2.0
import Qt3D.Render 2.0

Material {

    property Texture2D texture
    property vector2d textureScale: Qt.vector2d(1,-1)
    property vector2d textureBias: Qt.vector2d(0, 1)

    parameters: [
        Parameter { name: "surfaceTexture"; value: texture },
        Parameter { name: "texCoordScale"; value: textureScale },
        Parameter { name: "texCoordBias"; value: textureBias }
    ]

    effect: Effect {
        FilterKey {
            id: forward
            name: "renderingStyle"
            value: "forward"
        }

        ShaderProgram {
            id: gl2Es2Shader
            vertexShaderCode:   loadSource("qrc:/shaders/es2/texturing.vert")
            fragmentShaderCode: loadSource("qrc:/shaders/es2/texturing.frag")
        }

        ShaderProgram {
            id: gl3Shader
            vertexShaderCode:   loadSource("qrc:/shaders/gl3/texturing.vert")
            fragmentShaderCode: loadSource("qrc:/shaders/gl3/texturing.frag")
        }
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
                }
            },

            // OpenGL 2.1
            Technique {
                filterKeys: [ forward ]
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGL
                    profile: GraphicsApiFilter.NoProfile
                    majorVersion: 2
                    minorVersion: 0
                }

                renderPasses: RenderPass {
                    shaderProgram: gl2Es2Shader
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
                    shaderProgram: gl2Es2Shader
                }
            }
        ]
    }
}
