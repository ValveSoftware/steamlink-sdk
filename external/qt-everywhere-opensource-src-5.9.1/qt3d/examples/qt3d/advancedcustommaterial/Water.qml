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
import Qt3D.Input 2.0
import Qt3D.Extras 2.0
import QtQuick 2.0 as QQ2


 Entity {
    id: water

    WaterMaterial {
        id: watermaterial

        property real tox: 0.0
        property real toy: 0.0
        property real vertY: 1.0
        property real waveRandomAnim: 0.0

        diffuse: "qrc:/textures/WaterDiffuse.jpg"
        normal: "qrc:/textures/WaterNormal.jpg"
        specular: "qrc:/textures/WaterSpecular.jpg"
        wave: "qrc:/textures/Waterwave.jpg"
        sky: "qrc:/textures/sky.jpg"
        foam: "qrc:/textures/foam.jpg"

        textureScale: slider1.value
        wavescale: vertY * slider2.value
        specularity: slider3.value
        offsetx: tox * slider5.value
        offsety: toy * slider5.value
        normalAmount: slider8.value
        waveheight: slider6.value
        waveStrenght: slider7.value
        shininess: 100
        waveRandom: waveRandomAnim
    }

    Mesh {
        id: watermesh
        source: "qrc:/models/waterPlane.obj"
    }

    Transform {
        id: waterTransform
        property real scale: 1.0
        property real rotx: 0.0
        scale3D: Qt.vector3d(scale, scale, scale)
        rotationY: slider4.value
    }

    Entity {
        id: waterEntity
        components: [watermesh, watermaterial, waterTransform]
    }

    QQ2.SequentialAnimation {
        QQ2.NumberAnimation {
            target: watermaterial
            property: "waveRandomAnim"
            to: 3.0
            duration: 4000
            easing.type: Easing.Linear
        }
        QQ2.NumberAnimation {
            target: watermaterial
            property: "waveRandomAnim"
            to: 1.0
            duration: 4000
            easing.type: Easing.Linear
        }
    }

    QQ2.SequentialAnimation {
        running: true
        loops: QQ2.Animation.Infinite
        QQ2.ParallelAnimation {
            QQ2.NumberAnimation {
                target: watermaterial
                property: "toy"
                to: 10.0
                duration: 100000
            }
            QQ2.NumberAnimation {
                target: watermaterial
                property: "tox"
                to: 10.0
                duration: 100000
            }
        }
        QQ2.ParallelAnimation {
            QQ2.NumberAnimation {
                target: watermaterial
                property: "toy"
                to: 0.0
                duration: 0
            }
            QQ2.NumberAnimation {
                target: watermaterial
                property: "tox"
                to: 0.0
                duration: 0
            }
        }
    }

    QQ2.SequentialAnimation {
        running: true
        loops: QQ2.Animation.Infinite
        QQ2.NumberAnimation {
            target: watermaterial
            property: "vertY"
            to: 200
            duration: 200000
            easing.type: Easing.Linear
        }
        QQ2.NumberAnimation {
            target: watermaterial
            property: "vertY"
            to: 2
            duration: 200000
            easing.type: Easing.Linear
        }
    }
}

