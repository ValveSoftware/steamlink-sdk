/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import Qt3D.Core 2.0
import Qt3D.Render 2.0

Effect {
    id: root

    property Texture2D shadowTexture
    property Light light

    parameters: [
        Parameter { name: "lightViewProjection"; value: root.light.lightViewProjection },
        Parameter { name: "lightPosition";  value: root.light.lightPosition },
        Parameter { name: "lightIntensity"; value: root.light.lightIntensity },

        Parameter { name: "shadowMapTexture"; value: root.shadowTexture }
    ]

    techniques: [
        Technique {
            renderPasses: [
                RenderPass {
                    filterKeys: [ FilterKey { name: "pass"; value: "shadowmap" } ]

                    shaderProgram: ShaderProgram {
                        vertexShaderCode:   loadSource("qrc:/shaders/shadowmap.vert")
                        fragmentShaderCode: loadSource("qrc:/shaders/shadowmap.frag")
                    }

                    renderStates: [
                        PolygonOffset { scaleFactor: 4; units: 4 },
                        DepthTest { func: DepthTest.Less }
                    ]
                },

                RenderPass {
                    filterKeys: [ FilterKey { name: "pass"; value: "forward" } ]

                    bindings: [
                        ParameterMapping { parameterName: "ambient";    shaderVariableName: "ka"; bindingType: ParameterMapping.Uniform },
                        ParameterMapping { parameterName: "diffuse";    shaderVariableName: "kd"; bindingType: ParameterMapping.Uniform },
                        ParameterMapping { parameterName: "specular";   shaderVariableName: "ks"; bindingType: ParameterMapping.Uniform }
                    ]

                    shaderProgram: ShaderProgram {
                        vertexShaderCode:   loadSource("qrc:/shaders/ads.vert")
                        fragmentShaderCode: loadSource("qrc:/shaders/ads.frag")
                    }
                }
            ]
        }
    ]
}
