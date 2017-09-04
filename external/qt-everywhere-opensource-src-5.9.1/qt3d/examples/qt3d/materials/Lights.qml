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
    id: lights

    // Point Light (pulsing)
    Entity {
        components: [
            PointLight {
                color: "red"
                intensity: 0.3
                constantAttenuation: 1.0
                linearAttenuation: 0.0
                quadraticAttenuation: 0.0025

                NumberAnimation on intensity {
                    from: 0.3; to: 0.8;
                    running: true
                    loops: Animation.Infinite
                    duration: 1000
                    easing.type: Easing.CosineCurve
                }
            },
            Transform {
                translation: Qt.vector3d(0.0, 5.0, 0.0)
            }
        ]
    }

    // 2 x Directional Lights (steady)
    Entity {
        components: [
            DirectionalLight {
                worldDirection: Qt.vector3d(0.3, -3.0, 0.0).normalized();
                color: "#fbf9ce"
                intensity: 0.3
            }
        ]
    }

    Entity {
        components: [
            DirectionalLight {
                worldDirection: Qt.vector3d(-0.3, -0.3, 0.0).normalized();
                color: "#9cdaef"
                intensity: 0.15
            }
        ]
    }

    // Spot Light (sweeping)
    Entity {
        components: [
            SpotLight {
                localDirection: Qt.vector3d(0.0, -1.0, 0.0)
                color: "white"
                intensity: 0.6
            },
            Transform {
                id: spotLightTransform
                translation: Qt.vector3d(0.0, 5.0, 0.0)
                rotationZ: -60.0


                SequentialAnimation {
                    loops: Animation.Infinite
                    running: true

                    NumberAnimation {
                        target: spotLightTransform
                        property: "rotationZ"
                        from: -60; to: 60
                        duration: 2000
                        easing.type: Easing.InOutQuad
                    }

                    NumberAnimation {
                        target: spotLightTransform
                        property: "rotationZ"
                        from: 60; to: -60
                        duration: 2000
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        ]
    }
}
