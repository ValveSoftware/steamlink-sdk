/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

Effect {
    id : sceneMaterialEffect
    techniques : [
        // OpenGL 3.1
        Technique {
            graphicsApiFilter {api : GraphicsApiFilter.OpenGL; profile : GraphicsApiFilter.CoreProfile; minorVersion : 1; majorVersion : 3 }
            renderPasses : RenderPass {
                filterKeys : FilterKey { name : "pass"; value : "geometry" }
                shaderProgram : ShaderProgram {
                    id : sceneShaderGL3
                    vertexShaderCode:
                        "#version 140

                        in vec4 vertexPosition;
                        in vec3 vertexNormal;

                        out vec4 color0;
                        out vec3 position0;
                        out vec3 normal0;

                        uniform mat4 mvp;
                        uniform mat4 modelView;
                        uniform mat3 modelViewNormal;
                        uniform vec4 meshColor;

                        void main()
                        {
                            color0 = meshColor;
                            position0 = vec3(modelView * vertexPosition);
                            normal0 = normalize(modelViewNormal * vertexNormal);
                            gl_Position = mvp * vertexPosition;
                        }
                    "
                    fragmentShaderCode:
                        "#version 140

                        in vec4 color0;
                        in vec3 position0;
                        in vec3 normal0;

                        out vec4 color;
                        out vec3 position;
                        out vec3 normal;

                        void main()
                        {
                            color = color0;
                            position = position0;
                            normal = normal0;
                        }
                    "
                }
            }
        },
        // OpenGL 2.0
        Technique {
            graphicsApiFilter {api : GraphicsApiFilter.OpenGL; profile : GraphicsApiFilter.CoreProfile; minorVersion : 0; majorVersion : 2 }
            renderPasses : RenderPass {
                filterKeys : FilterKey { name : "pass"; value : "geometry" }
                shaderProgram : ShaderProgram {
                    id : sceneShaderGL2
                    vertexShaderCode:
                        "#version 110

                        attribute vec4 vertexPosition;
                        attribute vec3 vertexNormal;

                        varying vec4 color0;
                        varying vec3 position0;
                        varying vec3 normal0;

                        uniform mat4 mvp;
                        uniform mat4 modelView;
                        uniform mat3 modelViewNormal;
                        uniform vec4 meshColor;

                        void main()
                        {
                            color0 = meshColor;
                            position0 = vec3(modelView * vertexPosition);
                            normal0 = normalize(modelViewNormal * vertexNormal);
                            gl_Position = mvp * vertexPosition;
                        }
                    "
                    fragmentShaderCode:
                        "#version 110

                        varying vec4 color0;
                        varying vec3 position0;
                        varying vec3 normal0;

                        void main()
                        {
                            gl_FragData[0] = color0;
                            gl_FragData[1] = vec4(position0, 0);
                            gl_FragData[2] = vec4(normal0, 0);
                        }
                    "
                }
            }
        }
    ]
}
