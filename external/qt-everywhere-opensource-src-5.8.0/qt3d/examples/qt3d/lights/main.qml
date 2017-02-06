/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtQuick 2.0 as Quick
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity
{
    components: [
        RenderSettings {
            activeFrameGraph: ForwardRenderer {
                clearColor: Qt.rgba(0, 0, 0, 1)
                camera: camera
            }
        },
        InputSettings { }
    ]

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        position: Qt.vector3d( 0.0, 40.0, 300.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, -10.0, -1.0 )
    }

    FirstPersonCameraController {
        camera: camera
        linearSpeed: 1000.0
        acceleration: 0.1
        deceleration: 1.0
    }

    Entity {
        id: sun
        components: [
            DirectionalLight {
                color: Qt.rgba(0.8, 0.8, 0.8, 1.0)
                worldDirection: Qt.vector3d(-1, -1, 0)
            }
        ]
    }

    Entity {
        id: redLight
        components: [
            SphereMesh {
                radius: 2
            },
            Transform {
                translation: Qt.vector3d(2.0, 8.0, -2.0)
                Quick.SequentialAnimation on translation.y {
                    loops: Quick.Animation.Infinite
                    Quick.NumberAnimation { from: 8.0; to: 40.0; duration: 3000 }
                    Quick.NumberAnimation { from: 40.0; to: 8.0; duration: 3000 }
                }
            },
            PhongMaterial {
                diffuse: "red"
            },
            PointLight {
                color: Qt.rgba(1, 0, 0, 1)
            }
        ]
    }

    Entity {
        id: greenLight
        components: [
            SphereMesh {
                radius: 2
            },
            Transform {
                translation: Qt.vector3d(0.0, 3.0, 4.0)
                Quick.SequentialAnimation on translation.z {
                    loops: Quick.Animation.Infinite
                    Quick.NumberAnimation { from: 4.0; to: 40.0; duration: 5000 }
                    Quick.NumberAnimation { from: 40.0; to: 4.0; duration: 5000 }
                }
            },
            PhongMaterial {
                diffuse: "green"
            },
            PointLight {
                color: Qt.rgba(0, 1, 0, 1)
            }
        ]
    }

    Entity {
        id: spotLight
        components: [
            SphereMesh {
                radius: 1
            },
            Transform {
                translation: Qt.vector3d(-20.0, 40.0, 0.0)

                Quick.SequentialAnimation on translation {
                    loops: Quick.Animation.Infinite
                    running: true
                    Quick.Vector3dAnimation { from: Qt.vector3d(-40.0, 40.0, 0.0); to: Qt.vector3d(40.0, 40.0, 0.0); duration: 5000 }
                    Quick.Vector3dAnimation { from: Qt.vector3d(40.0, 40.0, 0.0); to: Qt.vector3d(-40.0, 40.0, 0.0); duration: 5000 }
                }
            },
            PhongMaterial {
                diffuse: "white"
            },
            SpotLight {
                localDirection: Qt.vector3d(0.0, -4.0, 0.0)
                Quick.SequentialAnimation on localDirection {
                    loops: Quick.Animation.Infinite
                    running: true
                    Quick.Vector3dAnimation { from: Qt.vector3d(0.0, -4.0, -4.0); to: Qt.vector3d(0.0, -4.0, 4.0); duration: 1000 }
                    Quick.Vector3dAnimation { from: Qt.vector3d(0.0, -4.0, 4.0); to: Qt.vector3d(0.0, -4.0, -4.0); duration: 1000 }
                }
                color: "white"
                cutOffAngle: 30
                constantAttenuation: 1
                intensity: 2.5
            }
        ]
    }

    PlaneEntity {
        id: floor

        width: 600
        height: 600
        resolution: Qt.size(20, 20)
        position: Qt.vector3d(0, -30, 0)

        material: NormalDiffuseMapMaterial {
            ambient: Qt.rgba( 0.2, 0.2, 0.2, 1.0 )
            diffuse: "assets/textures/pattern_09/diffuse.webp"
            normal: "assets/textures/pattern_09/normal.webp"
            textureScale: 10
            shininess: 10
        }
    }

    Entity {
        components: [
            PhongMaterial {
                diffuse: "white"
                shininess: 50
            },
            Mesh {
                source: "assets/obj/toyplane.obj"
            }
        ]
    }
}
