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

Viewport {
    id: root
    property alias camera: cameraSelector.camera
    property alias window: surfaceSelector.surface
    property alias clearColor: clearBuffer.clearColor

    readonly property Layer contentLayer: Layer {}
    readonly property Layer visualizationLayer: Layer {}
    readonly property Layer capsLayer: Layer {}

    RenderSurfaceSelector {
        id: surfaceSelector

        CameraSelector {
            id: cameraSelector

            RenderStateSet {
                // Enable 3 clipping planes
                renderStates: [
                    ClipPlane { planeIndex: 0 },
                    ClipPlane { planeIndex: 1 },
                    ClipPlane { planeIndex: 2 },
                    DepthTest { depthFunction: DepthTest.LessOrEqual }
                ]

                // Branch 1
                LayerFilter {
                    // Render entities with their regular material
                    // Fills depth buffer for entities that are clipped
                    layers: [root.contentLayer, root.visualizationLayer]
                    ClearBuffers {
                        id: clearBuffer
                        buffers: ClearBuffers.ColorDepthBuffer
                        RenderPassFilter {
                            matchAny: FilterKey { name: "pass"; value: "material" }
                        }
                    }
                }

                // Branch 2
                ClearBuffers {
                    // Enable and fill Stencil to later generate caps
                    buffers: ClearBuffers.StencilBuffer
                    RenderStateSet {
                        // Disable depth culling
                        // Incr for back faces
                        // Decr for front faces
                        // No need to output color values
                        renderStates: [
                            StencilTest {
                                front {
                                    stencilFunction: StencilTestArguments.Always
                                    referenceValue: 0; comparisonMask: 0
                                }
                                back {
                                    stencilFunction: StencilTestArguments.Always
                                    referenceValue: 0; comparisonMask: 0
                                }
                            },
                            StencilOperation {
                                front.allTestsPassOperation: StencilOperationArguments.Decrement
                                back.allTestsPassOperation: StencilOperationArguments.Increment
                            },
                            ColorMask { redMasked: false; greenMasked: false; blueMasked: false; alphaMasked: false }
                        ]

                        LayerFilter {
                            layers: root.contentLayer
                            RenderPassFilter {
                                matchAny: FilterKey { name: "pass"; value: "stencilFill"; }
                            }
                        }
                    }
                }
            }

            // Branch 3
            RenderStateSet {
                // Draw caps using stencil buffer
                LayerFilter {
                    layers: root.capsLayer
                    RenderPassFilter {
                        matchAny: FilterKey { name: "pass"; value: "capping"; }
                    }
                }

                // Draw back faces - front faces -> caps
                renderStates: [
                    StencilTest {
                        front {
                            stencilFunction: StencilTestArguments.NotEqual
                            referenceValue: 0; comparisonMask: ~0
                        }
                        back {
                            stencilFunction: StencilTestArguments.NotEqual
                            referenceValue: 0; comparisonMask: ~0
                        }
                    }
                ]
            }
        }
    }
}

