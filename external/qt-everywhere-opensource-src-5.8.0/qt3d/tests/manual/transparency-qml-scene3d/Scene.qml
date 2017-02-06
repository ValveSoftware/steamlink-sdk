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
import QtQuick 2.2 as QQ2

Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 800/600
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 40.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    FirstPersonCameraController { camera: camera }

    components: [
        RenderSettings {
            activeFrameGraph: ForwardRenderer{
                camera: camera
                clearColor: Qt.rgba(0.0, 0.5, 1, 1)
            }
        },
        InputSettings { }
    ]

    Entity {
        id: lightEntity
        property Transform transform: Transform {
            translation: Qt.vector3d(1.0, 1.0, 0.0)
        }

        property PointLight light: PointLight {
            id: light
            color: "white"
            constantAttenuation: 1.0
            linearAttenuation: 0.0
            quadraticAttenuation: 0.0
        }
        components: [transform, light]
    }

    SkyboxEntity {
        cameraPosition: camera.position
        baseName: "qrc:/assets/cubemaps/miramar/miramar"
        extension: ".webp"
    }

    TorusMesh {
        id: torusMesh
        radius: 5
        minorRadius: 1
        rings: 100
        slices: 20
    }

    SphereMesh {
        id: sphereMesh
        radius: 3
    }

    Transform {
        id: sphereTransform
        translation: Qt.vector3d(10, 0, 0)
    }

    Transform {
        id: cylinderTransform
        translation: Qt.vector3d(-10, 0, 0)
    }

    Entity {
        id: torusEntity
        components: [ torusMesh, phongMaterial ]
    }

    Entity {
        id: sphereEntity
        components: [ sphereMesh, alphaMaterial, sphereTransform ]
    }

    Entity {
        id: cylinderEntity
        components: [ sphereMesh, alphaMaterial, cylinderTransform ]
    }

    PhongAlphaMaterial {
        id: alphaMaterial
        shininess: 75.0
        ambient: "black"
        diffuse: "blue"
        specular: "white"
        alpha: 0.1
        sourceAlphaArg: BlendEquationArguments.Zero
        destinationAlphaArg: BlendEquationArguments.One

        QQ2.NumberAnimation {
            duration: 2000
            loops: QQ2.Animation.Infinite
            target: alphaMaterial
            property: "alpha"
            from: 0.0
            to: 1.0
            running: true
        }
    }

    PhongMaterial {
        id: phongMaterial
    }
}
