/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

import Qt3D.Core 2.9
import Qt3D.Render 2.9
import Qt3D.Extras 2.9
import Qt3D.Input 2.0

import QtQuick 2.0
import QtQuick.Scene2D 2.9
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2

Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        position: Qt.vector3d( 0.0, 0.0, 20 )
    }

    components: [
        RenderSettings {
            activeFrameGraph: ForwardRenderer {
                camera: camera
                clearColor: "white"
            }
            pickingSettings.pickMethod: PickingSettings.TrianglePicking
        },
        InputSettings {}
    ]

    Entity {
        id: logoEntity

        Transform {
            id: logoTransform
            scale: 1
            translation: Qt.vector3d( 0, 0, logoControls.logoCentreZ )
            rotation: fromEulerAngles( logoControls.rotationX,
                                       logoControls.rotationY,
                                       logoControls.rotationZ )
        }

        Mesh {
            id: logoMesh
            source: "Qt_logo.obj"
        }

        PhongMaterial {
            id: logoMaterial
            ambient: Qt.rgba( logoControls.colorR/255,
                              logoControls.colorG/255,
                              logoControls.colorB/255, 1.0 )
            diffuse: Qt.rgba( 0.1, 0.1, 0.1, 0.5 )
            shininess: logoControls.shininess
        }

        components: [ logoTransform, logoMesh, logoMaterial ]
    }

    Entity {
        id: cube

        components: [cubeTransform, cubeMaterial, cubeMesh, cubePicker]

        property real rotationAngle: 0

        Behavior on rotationAngle {
            enabled: logoControls.enabled
            RotationAnimation {
                direction: RotationAnimation.Shortest
                duration: 450
            }
        }

        RotationAnimation on rotationAngle {
            running: !logoControls.enabled
            loops: Animation.Infinite
            from: 0; to: 360
            duration: 4000
            onStopped: cube.rotationAngle = 0
        }

        Transform {
            id: cubeTransform
            translation: Qt.vector3d(2, 0, 10)
            scale3D: Qt.vector3d(1, 4, 1)
            rotation: fromAxisAndAngle(Qt.vector3d(0,1,0), cube.rotationAngle)
        }

        CuboidMesh {
            id: cubeMesh
        }

        ObjectPicker {
            id: cubePicker
            hoverEnabled: true
            dragEnabled: true

            // Explicitly require a middle click to have the Scene2D grab the mouse
            // events from the picker
            onPressed: {
                if (pick.button === PickEvent.MiddleButton) {
                    qmlTexture.mouseEnabled = !qmlTexture.mouseEnabled
                    logoControls.enabled = !logoControls.enabled
                }
            }
        }

        TextureMaterial {
            id: cubeMaterial
            texture: offscreenTexture
        }

        Scene2D {
            id: qmlTexture
            output: RenderTargetOutput {
                attachmentPoint: RenderTargetOutput.Color0
                texture: Texture2D {
                    id: offscreenTexture
                    width: 256
                    height: 1024
                    format: Texture.RGBA8_UNorm
                    generateMipMaps: true
                    magnificationFilter: Texture.Linear
                    minificationFilter: Texture.LinearMipMapLinear
                    wrapMode {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                }
            }

            entities: [ cube ]
            mouseEnabled: false

            LogoControls {
                id: logoControls
                width: offscreenTexture.width
                height: offscreenTexture.height
            }
        }
    }
}
