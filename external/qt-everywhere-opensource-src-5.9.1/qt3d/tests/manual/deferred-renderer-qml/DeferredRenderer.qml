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

Viewport {
    id: root
    normalizedRect : Qt.rect(0.0, 0.0, 1.0, 1.0)

    property GBuffer gBuffer
    property alias camera : sceneCameraSelector.camera
    property alias sceneLayer: sceneLayerFilter.layers
    property alias screenQuadLayer: screenQuadLayerFilter.layers
    property alias debugLayer: debugLayerFilter.layers

    readonly property real windowWidth: surfaceSelector.surface !== null ? surfaceSelector.surface.width: 0
    readonly property real windowHeight: surfaceSelector.surface !== null ? surfaceSelector.surface.height: 0

    RenderSurfaceSelector {
        id: surfaceSelector

        CameraSelector {
            id : sceneCameraSelector

            // Fill G-Buffer
            LayerFilter {
                id: sceneLayerFilter
                RenderTargetSelector {
                    id : gBufferTargetSelector
                    target: gBuffer

                    ClearBuffers {
                        buffers: ClearBuffers.ColorDepthBuffer

                        RenderPassFilter {
                            id : geometryPass
                            matchAny : FilterKey { name : "pass"; value : "geometry" }
                        }
                    }
                }
            }

            TechniqueFilter {
                parameters: [
                    Parameter { name: "color"; value : gBuffer.color },
                    Parameter { name: "position"; value : gBuffer.position },
                    Parameter { name: "normal"; value : gBuffer.normal },
                    Parameter { name: "depth"; value : gBuffer.depth },
                    Parameter { name: "winSize"; value : Qt.size(1024, 1024) }
                ]

                RenderStateSet {
                    // Render FullScreen Quad
                    renderStates: [
                        BlendEquation { blendFunction: BlendEquation.Add },
                        BlendEquationArguments { sourceRgb: BlendEquationArguments.SourceAlpha; destinationRgb: BlendEquationArguments.DestinationColor }
                    ]
                    LayerFilter {
                        id: screenQuadLayerFilter
                        ClearBuffers {
                            buffers: ClearBuffers.ColorDepthBuffer
                            RenderPassFilter {
                                matchAny : FilterKey { name : "pass"; value : "final" }
                                parameters: Parameter { name: "winSize"; value : Qt.size(windowWidth, windowHeight) }

                            }
                        }
                    }
                    // RenderDebug layer
                    LayerFilter {
                        id: debugLayerFilter
                        Viewport {
                            normalizedRect : Qt.rect(0.5, 0.5, 0.5, 0.5)
                            RenderPassFilter {
                                matchAny : FilterKey { name : "pass"; value : "final" }
                                parameters: Parameter { name: "winSize"; value : Qt.size(windowWidth * 0.5, windowHeight * 0.5) }
                            }
                        }
                    }
                }
            }
        }
    }
}
