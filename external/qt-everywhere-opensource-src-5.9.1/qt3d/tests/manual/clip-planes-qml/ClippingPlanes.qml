/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
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

import QtQuick 2.4 as QQ2
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0

Entity {
    id: root
    property Layer capsLayer
    property Layer visualizationLayer

    property ShaderData sectionData: ShaderData {
        property int sectionsCount: 3
        property ShaderDataArray sections: ShaderDataArray {
            ShaderData {
                property vector4d equation: clipPlane0.equation
                property vector3d center: clipPlane0.center
            }
            ShaderData {
                property vector4d equation: clipPlane1.equation
                property vector3d center: clipPlane1.center
            }
            ShaderData {
                property vector4d equation: clipPlane2.equation
                property vector3d center: clipPlane2.center
            }
        }
    }

    Entity {
        property Material cappingMaterial: Material {
            effect: CappingMaterialEffect {
                sectionsData: root.sectionData
            }
        }

        property PlaneMesh mesh: PlaneMesh {
            width: 20.0
            height: 20.0
            meshResolution: Qt.size(2, 2)
        }

        components: [cappingMaterial, mesh, root.capsLayer]
    }

    PlaneVisualizationMaterial {
        id: clipPlanesMaterial
        alpha: 0.5
    }

    // XZ
    ClipPlaneEntity {
        id: clipPlane0
        layer: root.visualizationLayer
        visualMaterial: clipPlanesMaterial
        center: Qt.vector3d(0, -10, 0)
        normal: Qt.vector3d(0, -1.0, 0)
    }
    // XY
    ClipPlaneEntity {
        id: clipPlane1
        layer: root.visualizationLayer
        visualMaterial: clipPlanesMaterial
        center: Qt.vector3d(0, 0, 10)
        normal: Qt.vector3d(0, 0, -1.0)
        rotateAxis: Qt.vector3d(1.0, 0.0, 0.0)
        rotateAngle: 90
    }

    // YZ
    ClipPlaneEntity {
        id: clipPlane2
        layer: root.visualizationLayer
        visualMaterial: clipPlanesMaterial
        center: Qt.vector3d(-10, 0, 0)
        normal: Qt.vector3d(1.0, 0, 0)
        rotateAxis: Qt.vector3d(0.0, 0.0, 1.0)
        rotateAngle: -90
    }

    QQ2.SequentialAnimation {
        running: true
        loops: QQ2.Animation.Infinite
        QQ2.NumberAnimation {
            target: clipPlane0
            property: "center.y"
            from: 10
            to: 0
            duration: 2000
            easing.type: QQ2.Easing.InOutQuart
        }

        QQ2.NumberAnimation {
            target: clipPlane1
            property: "center.z"
            from: 10
            to: 0
            duration: 2000
            easing.type: QQ2.Easing.InOutQuart
        }
        QQ2.NumberAnimation {
            target: clipPlane2
            property: "center.x"
            from: -10
            to: 0
            duration: 4000
            easing.type: QQ2.Easing.InOutQuart
        }
        QQ2.NumberAnimation {
            target: clipPlane1
            property: "center.z"
            from: 0
            to: 10
            duration: 4000
            easing.type: QQ2.Easing.InOutQuint
        }
        QQ2.NumberAnimation {
            target: clipPlane0
            property: "center.y"
            from: 0
            to: 10
            duration: 2000
            easing.type: QQ2.Easing.InOutQuint
        }
        QQ2.NumberAnimation {
            target: clipPlane2
            property: "center.x"
            from: 0
            to: -10
            duration: 2000
            easing.type: QQ2.Easing.InOutQuint
        }
    }
}
