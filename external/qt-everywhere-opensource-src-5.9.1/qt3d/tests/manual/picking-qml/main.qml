/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

import QtQuick 2.0 as QQ2
import QtQuick.Window 2.2 as W
import QtQuick.Scene3D 2.0

Entity {
    id: sceneRoot

    property Layer contentLayer: Layer {}
    property Layer debugLayer: Layer {}

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: _view.width * 0.5 / _view.height
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, -40.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    Camera {
        id: camera2
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: _view.width * 0.5 / _view.height
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 40.0, 5.0, -20.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    FirstPersonCameraController {
        camera: camera
    }

    // Draw 2 viewports
    // one with the content, the other with content + debug volumes
    components: [
        RenderSettings {
            Viewport {
                normalizedRect: Qt.rect(0.0, 0.0, 1.0, 1.0)

                RenderSurfaceSelector {
                    ClearBuffers {
                        buffers : ClearBuffers.ColorDepthBuffer
                        NoDraw {}
                    }

                    Viewport {
                        normalizedRect: Qt.rect(0.0, 0.0, 0.5, 1.0)
                        CameraSelector {
                            camera: camera
                            LayerFilter { layers: sceneRoot.contentLayer }
                        }
                    }

                    Viewport {
                        normalizedRect: Qt.rect(0.5, 0.0, 0.5, 1.0)
                        CameraSelector {
                            camera: camera2
                            LayerFilter {
                                // To show Debug volumes
                                layers: [sceneRoot.contentLayer, sceneRoot.debugLayer]
                            }
                        }
                    }
                }
            }
        },
        InputSettings {}
    ]

    CuboidMesh { id: cubeMesh }

    PickableEntity {
        id: cube1
        property real scaleFactor: isPressed ? 3.0 : 1.0
        QQ2.Behavior on scaleFactor { QQ2.NumberAnimation { duration: 150; easing.type: QQ2.Easing.InQuad } }

        layer: sceneRoot.contentLayer
        mesh: cubeMesh
        scale: cube1.scaleFactor
        x: -8

        ambientColor: "green"
        diffuseColor: "green"

        hoverEnabled: true
        onPressed: cube1.diffuseColor = "orange"
        onReleased:  cube1.diffuseColor = "green"
        onEntered: cube1.ambientColor = "blue"
        onExited: cube1.ambientColor = "green"
        onClicked: console.log("Clicked cube 1")
    }

    PickableEntity {
        id: cube2
        layer: sceneRoot.contentLayer
        mesh: cubeMesh

        ambientColor: cube2.containsMouse ? "blue" : "red"
        scale: 1.5
        hoverEnabled: true

        onPressed: cube2.diffuseColor = "white"
        onReleased: cube2.diffuseColor = "red"

        property bool toggled: false
        onClicked: {
            console.log("Clicked cube 2", event.button)
            toggled = !toggled
        }
    }
    PickableEntity {
        id: cube3
        layer: sceneRoot.contentLayer
        mesh: cubeMesh

        diffuseColor: "yellow"

        property bool toggled: false
        property real scaleFactor: toggled ? 5.0 : 0.5
        QQ2.Behavior on scaleFactor { QQ2.NumberAnimation { duration: 200; easing.type: QQ2.Easing.InQuad } }

        scale: cube3.scaleFactor
        x: 8

        onPressed: cube3.toggled = !cube3.toggled
        onEntered: cube3.ambientColor = "black"
        onExited: cube3.ambientColor = "white"
        onClicked: console.log("Clicked cube 3")
    }

    Entity {
        readonly property ObjectPicker objectPicker: ObjectPicker {
            hoverEnabled: true
            onPressed: cube4.toggled = !cube4.toggled
            onClicked: console.log("Clicked cube 4's parent Entity")
            onEntered: cube4.material.diffuse = "white"
            onExited: cube4.material.diffuse = "blue"
        }

        components: [objectPicker]

        Entity {
            id: cube4
            property bool toggled: false
            property real scaleFactor: toggled ? 2.0 : 1.0
            QQ2.Behavior on scaleFactor { QQ2.NumberAnimation { duration: 200; easing.type: QQ2.Easing.InQuad } }

            readonly property Transform transform: Transform {
                scale: cube4.scaleFactor
                translation: Qt.vector3d(3, 4, 0)
            }
            readonly property PhongMaterial material: PhongMaterial { diffuse: "red" }

            components: [cubeMesh, transform, material, sceneRoot.contentLayer]
        }
    }
}
