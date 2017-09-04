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

import Qt3D.Core 2.0
import Qt3D.Render 2.9
import Qt3D.Input 2.0
import Qt3D.Extras 2.9

import QtQuick 2.0 as QQ2

Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 20.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    OrbitCameraController { camera: camera }

    Entity {
        id: headlight
        components: [
            DirectionalLight {
                color: Qt.rgba(0.8, 0.8, 0.8, 1.0)
                worldDirection: camera.viewVector
            }
        ]
    }

    RenderSettings {
        id : external_forward_renderer
        activeFrameGraph : ForwardRenderer {
            camera: camera
            clearColor: "white"
        }
    }

    // Event Source will be set by the Qt3DQuickWindow
    InputSettings { id: inputSettings }

    components: [external_forward_renderer, inputSettings]

    Entity {
        components: [
            Transform {
                id: transform
            }
        ]

        Entity {
            components: [
                CylinderMesh {
                    id: mesh
                    radius: 1
                    length: 3
                    rings: 2
                    slices: sliceValues[lod.currentIndex]

                    property var sliceValues: [20, 10, 6, 4]
                },
                Transform {
                    scale: 1.5
                    translation: Qt.vector3d(0, 0, 0)
                    rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 45)
                },
                PhongMaterial {
                    id: material
                    diffuse: "lightgreen"
                },
                LevelOfDetail {
                    id: lod
                    camera: camera
                    thresholds: [1000, 600, 300, 180]
                    thresholdType: LevelOfDetail.ProjectedScreenPixelSizeThreshold
                    volumeOverride: lod.createBoundingSphere(Qt.vector3d(0, 0, 0), 2.0)
                }
            ]
        }
    }

    Entity {
        components: [
            Transform {
                translation: transform.translation
            }
        ]

        LevelOfDetailLoader {
            id: lodLoader
            components: [ Transform {
                    scale: .5
                    translation: Qt.vector3d(-8, 0, 0)
                } ]

            camera: camera
            thresholds: [20, 35, 50, 65]
            thresholdType: LevelOfDetail.DistanceToCameraThreshold
            volumeOverride: lodLoader.createBoundingSphere(Qt.vector3d(0, 0, 0), -1)
            sources: ["qrc:/SphereEntity.qml", "qrc:/CylinderEntity.qml", "qrc:/ConeEntity.qml", "qrc:/CuboidEntity.qml"]
        }
    }

    Entity {
        components: [
            Transform {
                translation: transform.translation
            }
        ]

        Entity {
            components: [
                Transform {
                    scale: .5
                    translation: Qt.vector3d(8, 0, 0)
                },
                LevelOfDetailSwitch {
                    camera: camera
                    thresholds: [20, 35, 50, 65]
                    thresholdType: LevelOfDetail.DistanceToCameraThreshold
                }
            ]

            SphereEntity {
                enabled: false
            }
            CylinderEntity {
                enabled: false
            }
            ConeEntity {
                enabled: false
            }
            CuboidEntity {
                enabled: false
            }
        }
    }

    QQ2.SequentialAnimation {
        QQ2.Vector3dAnimation {
            target: transform
            properties: "translation"
            from: Qt.vector3d(0, 0, 10)
            to: Qt.vector3d(0, 0, -50)
            duration: 2500
            easing.type: QQ2.Easing.InOutQuad
        }
        QQ2.PropertyAnimation {
            target: transform
            properties: "translation"
            from: Qt.vector3d(0, 0, -50)
            to: Qt.vector3d(0, 0, 10)
            duration: 2500
            easing.type: QQ2.Easing.InOutQuad
        }

        loops: QQ2.Animation.Infinite
        running: true
    }
}
