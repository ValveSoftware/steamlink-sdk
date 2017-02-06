/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

import QtQuick 2.1
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity {
    id: root
    objectName: "root"

    // Use the renderer configuration specified in ForwardRenderer.qml
    // and render from the mainCamera
    components: [
        RenderSettings {
            activeFrameGraph: SortedForwardRenderer {
                id: renderer
                camera: mainCamera
            }
        },
        // Event Source will be set by the Qt3DQuickWindow
        InputSettings { }
    ]

    BasicCamera {
        id: mainCamera
        position: Qt.vector3d( 0.0, 3.5, 25.0 )
        viewCenter: Qt.vector3d( 0.0, 3.5, 0.0 )
    }

    FirstPersonCameraController { camera: mainCamera }

    Lights { }

    PhongMaterial {
        id: redAdsMaterial
        ambient: Qt.rgba( 0.02, 0.02, 0.02, 1.0 )
        diffuse: Qt.rgba( 0.8, 0.0, 0.0, 1.0 )
    }

    PlaneEntity {
        id: floor

        width: 100
        height: 100
        resolution: Qt.size(20, 20)

        material: NormalDiffuseSpecularMapMaterial {
            ambient: Qt.rgba( 0.05, 0.05, 0.05, 1.0 )
            diffuse:  "assets/textures/pattern_09/diffuse.webp"
            specular: "assets/textures/pattern_09/specular.webp"
            normal:   "assets/textures/pattern_09/normal.webp"
            textureScale: 10.0
            shininess: 10.0
        }
    }

    TrefoilKnot {
        id: trefoilKnot
        material: redAdsMaterial
        y: 3.5
        scale: 0.5

        ParallelAnimation {
            loops: Animation.Infinite
            running: true

            NumberAnimation {
                target: trefoilKnot
                property: "theta"
                from: 0; to: 360
                duration: 2000
            }

            NumberAnimation {
                target: trefoilKnot
                property: "phi"
                from: 0; to: 360
                duration: 2000
            }
        }
    }

    Chest {
        x: -8
    }

    HousePlant {
        x: 4
        potShape: "square"
        plantType: "bamboo"
    }

    HousePlant {
        z: 4
        potShape: "triangle"
        plantType: "palm"
    }

    HousePlant {
        x: -4
        potShape: "sphere"
        plantType: "pine"
    }

    HousePlant {
        z: -4
        potShape: "cross"
        plantType: "spikes"
    }

    HousePlant {
        z: -8
        potShape: "cross"
        plantType: "palm"
        scale: 1.15
    }

    HousePlant {
        z: 8
        potShape: "cross"
        plantType: "shrub"
        scale: 1.15
    }

    Barrel {
        x: 8
    }

    Barrel {
        x: 10
        diffuseColor: "rust"
        bump: "hard_bumps"
        specular: "rust"
    }

    Barrel {
        x: 12
        diffuseColor: "blue"
        bump: "middle_bumps"
    }

    Barrel {
        x: 14
        diffuseColor: "green"
        bump: "soft_bumps"
    }

    Barrel {
        x: 16
        diffuseColor: "stainless_steel"
        bump: "no_bumps"
        specular: "stainless_steel"
        shininess: 15
    }
}
