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

import QtQuick 2.0
import QtQuick.Scene3D 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

Item {

    Scene3D {
        anchors.fill: parent
        aspects: "input"
        ParticlesScene {
            id: scene
            particleStep: stepSlider.value
            finalCollisionFactor: collisionSlider.value
        }
    }

    ColumnLayout {
        id: colorLayout
        anchors.left: parent.left
        anchors.leftMargin: 35
        anchors.right: parent.right
        anchors.rightMargin: 35
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 35
        spacing: 15

        RowLayout {
            Text {
                text: "Particles Step:"
                color: "white"
            }
            Slider {
                height: 35
                id: stepSlider
                Layout.fillWidth: true
                minimumValue: 0.0
                maximumValue: 2
                value: 0.4
            }
        }
        RowLayout {
            Text {
                text: "Particles Collision:"
                color: "white"
            }
            Slider {
                height: 35
                id: collisionSlider
                Layout.fillWidth: true
                minimumValue: 0.0
                maximumValue: 2
                value: 0.2
            }
        }
        RowLayout {
            Button {
                text: "Reset Particles"
               onClicked: scene.reset()
            }
        }
        RowLayout {
            Text {
                text: "Particles Shape:"
                color: "white"
            }
            ExclusiveGroup {
                id: particlesTypeGroup
            }
            CheckBox {
                text: "Sphere"
                checked: true
                exclusiveGroup: particlesTypeGroup
                onClicked: scene.particlesShape = scene._SPHERE
            }
            CheckBox
            { text: "Cube"
                checked: false
                exclusiveGroup: particlesTypeGroup
                onClicked: scene.particlesShape = scene._CUBE
            }
            CheckBox {
                text: "Cylinder"
                checked: false
                exclusiveGroup: particlesTypeGroup
                onClicked: scene.particlesShape = scene._CYLINDER
            }
            CheckBox {
                text: "Torus"
                checked: false
                exclusiveGroup: particlesTypeGroup
                onClicked: scene.particlesShape = scene._TORUS
            }
        }
    }
}
