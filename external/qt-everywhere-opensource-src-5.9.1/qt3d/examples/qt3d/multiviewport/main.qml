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

import QtQuick 2.0
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity {
    id: rootNode
    components: [quadViewportFrameGraph, inputSettings]

    QuadViewportFrameGraph {
        id: quadViewportFrameGraph
        topLeftCamera: cameraSet.cameras[0]
        topRightCamera: cameraSet.cameras[1]
        bottomLeftCamera: cameraSet.cameras[2]
        bottomRightCamera: cameraSet.cameras[3]
    }

    // Event Source will be set by the Qt3DQuickWindow
    InputSettings { id: inputSettings }

    Entity {
        id: cameraSet
        property var cameras: [camera1, camera2, camera3, camera4]

        CameraLens {
            id: cameraLens
            projectionType: CameraLens.PerspectiveProjection
            fieldOfView: 45
            aspectRatio: 16/9
            nearPlane: 0.01
            farPlane: 1000.0
        }
        CameraLens {
            id: cameraLens2
            projectionType: CameraLens.PerspectiveProjection
            fieldOfView: 15
            aspectRatio: 16/9
            nearPlane: 0.01
            farPlane: 1000.0
        }
        CameraLens {
            id: cameraLens3
            projectionType: CameraLens.PerspectiveProjection
            fieldOfView: 5
            aspectRatio: 16/9
            nearPlane: 0.01
            farPlane: 1000.0
        }

        SimpleCamera {
            id: camera1
            lens: cameraLens2
            position: Qt.vector3d(10.0, 1.0, 10.0)
            viewCenter: Qt.vector3d(0.0, 1.0, 0.0)
        }

        SimpleCamera {
            id: camera2
            lens: cameraLens
            position: Qt.vector3d(0.0, 0.0, 5.0)
            viewCenter: Qt.vector3d(0.0, 0.0, 0.0)
        }

        SimpleCamera {
            id: camera3
            lens: cameraLens2
            position: Qt.vector3d(30.0, 30.0, 20.0)
            viewCenter: Qt.vector3d(0.0, 0.0, -8.0)
        }

        SimpleCamera {
            id: camera4
            lens: cameraLens3
            position: Qt.vector3d(100.0, 0.0, -6.0)
            viewCenter: Qt.vector3d(0.0, 0.0, -6.0)
        }
    }

    Entity {
        id: sceneRoot
        property real rotationAngle: 0

        SequentialAnimation {
            running: true
            loops: Animation.Infinite
            NumberAnimation { target: sceneRoot; property: "rotationAngle"; to: 360; duration: 4000; }
        }

        Entity {
            components: [
                Transform {
                    rotation: fromAxisAndAngle(Qt.vector3d(0, 0, 1), -sceneRoot.rotationAngle)
                },
                SceneLoader {
                    source: "qrc:/Gear_scene.dae"
                }
            ]
        }
    } // sceneRoot
} // rootNode
