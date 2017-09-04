/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
import QtQuick 2.2 as QQ2

Entity {
    id: sceneRoot
    property int barRotationTimeMs: 1
    property int numberOfBars: 1
    property string animationState: "stopped"
    property real titleStartAngle: 95
    property real titleStopAngle: 5

    onAnimationStateChanged: {
        if (animationState == "playing") {
            mediaPlayer.play()
            if (progressTransformAnimation.paused)
                progressTransformAnimation.resume()
            else
                progressTransformAnimation.start()
        } else if (animationState == "paused") {
            mediaPlayer.pause()
            if (progressTransformAnimation.running)
                progressTransformAnimation.pause()
        } else {
            mediaPlayer.stop()
            progressTransformAnimation.stop()
            progressTransform.progressAngle = progressTransform.defaultStartAngle
        }
    }

    QQ2.Item {
        id: stateItem

        state: animationState
        states: [
            QQ2.State {
                name: "playing"
                QQ2.PropertyChanges {
                    target: titlePrism
                    titleAngle: titleStopAngle
                }
            },
            QQ2.State {
                name: "paused"
                QQ2.PropertyChanges {
                    target: titlePrism
                    titleAngle: titleStopAngle
                }
            },
            QQ2.State {
                name: "stopped"
                QQ2.PropertyChanges {
                    target: titlePrism
                    titleAngle: titleStartAngle
                }
            }
        ]

        transitions: QQ2.Transition {
            QQ2.NumberAnimation {
                property: "titleAngle"
                duration: 2000
                running: false
            }
        }
    }

    function startVisualization() {
        progressTransformAnimation.duration = mediaPlayer.duration
        mainview.state = "playing"
        progressTransformAnimation.start()
    }

    //![0]
    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 1820 / 1080
        nearPlane: 0.1
        farPlane: 1000.0
        position: Qt.vector3d(0.014, 0.956, 2.178)
        upVector: Qt.vector3d(0.0, 1.0, 0.0)
        viewCenter: Qt.vector3d(0.0, 0.7, 0.0)
    }
    //![0]

    Entity {
        components: [
            DirectionalLight {
                intensity: 0.9
                worldDirection: Qt.vector3d(0, 0.6, -1)
            }
        ]
    }

    RenderSettings {
        id: external_forward_renderer
        activeFrameGraph: ForwardRenderer {
            camera: camera
            clearColor: "transparent"
        }
    }

    components: [external_forward_renderer]

    //![1]
    // Bars
    CuboidMesh {
        id: barMesh
        xExtent: 0.1
        yExtent: 0.1
        zExtent: 0.1
    }

    NodeInstantiator {
        id: collection
        property int maxCount: parent.numberOfBars
        model: maxCount

        delegate: BarEntity {
            id: cubicEntity
            entityMesh: barMesh
            rotationTimeMs: sceneRoot.barRotationTimeMs
            entityIndex: index
            entityCount: sceneRoot.numberOfBars
            entityAnimationsState: animationState
            magnitude: 0
        }
    }
    //![1]

    // TitlePrism
    Entity {
        id: titlePrism
        property real titleAngle: titleStartAngle

        Entity {
            id: titlePlane

            PlaneMesh {
                id: titlePlaneMesh
                width: 550
                height: 100
            }

            Transform {
                id: titlePlaneTransform
                scale: 0.003
                translation: Qt.vector3d(0, 0.11, 0)
            }

            NormalDiffuseMapAlphaMaterial {
                id: titlePlaneMaterial
                diffuse: TextureLoader { source: "qrc:/images/demotitle.png" }
                normal: TextureLoader { source: "qrc:/images/normalmap.png" }
                shininess: 1.0
            }

            components: [titlePlaneMesh, titlePlaneMaterial, titlePlaneTransform]
        }

        // Song title
        Entity {
            id: songTitlePlane

            PlaneMesh {
                id: songPlaneMesh
                width: 550
                height: 100
            }

            Transform {
                id: songPlaneTransform
                scale: 0.003
                rotationX: 90
                translation: Qt.vector3d(0, -0.03, 0.13)
            }

            property Material songPlaneMaterial: NormalDiffuseMapAlphaMaterial {
                diffuse: TextureLoader { source: "qrc:/images/songtitle.png" }
                normal: TextureLoader { source: "qrc:/images/normalmap.png" }
                shininess: 1.0
            }

            components: [songPlaneMesh, songPlaneMaterial, songPlaneTransform]
        }

        property Transform titlePrismPlaneTransform: Transform {
            matrix: {
                var m = Qt.matrix4x4()
                m.translate(Qt.vector3d(-0.5, 1.3, -0.4))
                m.rotate(titlePrism.titleAngle, Qt.vector3d(1, 0, 0))
                return m;
            }
        }

        components: [titlePlane, songTitlePlane, titlePrismPlaneTransform]
    }

    // Circle to create the reflection effect
    Mesh {
        id: circleMesh
        source: "qrc:/meshes/circle.obj"
    }

    Entity {
        id: circleEntity
        property Material circleMaterial: PhongAlphaMaterial {
            alpha: 0.4
            ambient: "black"
            diffuse: "black"
            specular: "black"
            shininess: 10000
        }

        components: [circleMesh, circleMaterial]
    }

    //![2]
    // Progress
    Mesh {
        id: progressMesh
        source: "qrc:/meshes/progressbar.obj"
    }

    Transform {
        id: progressTransform
        property real defaultStartAngle: -90
        property real progressAngle: defaultStartAngle
        rotationY: progressAngle
    }

    Entity {
        property Material progressMaterial: PhongMaterial {
            ambient: "#80C342"
            diffuse: "black"
        }

        components: [progressMesh, progressMaterial, progressTransform]
    }
    //![2]

    QQ2.NumberAnimation {
        id: progressTransformAnimation
        target: progressTransform
        property: "progressAngle"
        duration: 0
        running: false
        from: progressTransform.defaultStartAngle
        to: -270
        onStopped: if (animationState != "stopped") animationState = "stopped"
    }
}
