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

import QtQuick 2.1 as QQ2
import Qt3D.Core 2.0
import Qt3D.Render 2.0

Entity {
    id: root
    property Material material

    Mesh {
        id: toyplaneMesh
        source: "assets/obj/toyplane.obj"
    }

    Transform {
        id: toyplaneTransform

        property real rollAngle: 0
        property real pitchAngle: 15
        property real altitude: 5
        property real angle: 0
        property real scaleFactor: 10

        QQ2.Behavior on rollAngle { QQ2.SpringAnimation { spring: 2; damping: 0.2} }

        matrix: {
            var m = Qt.matrix4x4();
            m.translate(Qt.vector3d(Math.sin(angle * Math.PI / 180) * scaleFactor,
                                    altitude,
                                    Math.cos(angle * Math.PI / 180) * scaleFactor));
            m.rotate(angle, Qt.vector3d(0, 1, 0));
            m.rotate(pitchAngle, Qt.vector3d(0, 0, 1));
            m.rotate(rollAngle, Qt.vector3d(1, 0, 0));
            m.scale(1.0 / toyplaneTransform.scaleFactor);
            return m;
        }
    }

    QQ2.NumberAnimation {
        target: toyplaneTransform

        running: true
        loops: QQ2.Animation.Infinite

        property: "angle"
        duration: 10000
        from: 0
        to: 360
    }

    // Altitude / Pitch animation
    QQ2.SequentialAnimation {
        running: true
        loops: QQ2.Animation.Infinite
        QQ2.ParallelAnimation {
            QQ2.SequentialAnimation {
                QQ2.NumberAnimation { target: toyplaneTransform; property: "pitchAngle"; from: 0; to: 30; duration: 2000; easing.type: QQ2.Easing.OutQuad }
                QQ2.NumberAnimation { target: toyplaneTransform; property: "pitchAngle"; from: 30; to: 0; duration: 2000; easing.type: QQ2.Easing.OutSine }
            }
            QQ2.NumberAnimation { target: toyplaneTransform; property: "altitude"; to: 5; duration: 4000; easing.type: QQ2.Easing.InOutCubic }
        }
        QQ2.PauseAnimation { duration: 1500 }
        QQ2.ParallelAnimation {
            QQ2.SequentialAnimation {
                QQ2.NumberAnimation { target: toyplaneTransform; property: "pitchAngle"; from: 0; to: -30; duration: 1000; easing.type: QQ2.Easing.OutQuad }
                QQ2.NumberAnimation { target: toyplaneTransform; property: "pitchAngle"; from: -30; to: 0; duration: 5000; easing.type: QQ2.Easing.OutSine }
            }
            QQ2.NumberAnimation { target: toyplaneTransform; property: "altitude"; to: 0; duration: 6000; easing.type: QQ2.Easing.InOutCubic}
        }
        QQ2.PauseAnimation { duration: 1500 }
    }

    // Roll Animation
    QQ2.SequentialAnimation {
        running: true
        loops: QQ2.Animation.Infinite
        QQ2.NumberAnimation { target: toyplaneTransform; property: "rollAngle"; to: 360; duration: 1500; easing.type: QQ2.Easing.InOutQuad }
        QQ2.PauseAnimation { duration: 1000 }
        QQ2.NumberAnimation { target: toyplaneTransform; property: "rollAngle"; from: 0; to: 30; duration: 1000; easing.type: QQ2.Easing.OutQuart }
        QQ2.PauseAnimation { duration: 1500 }
        QQ2.NumberAnimation { target: toyplaneTransform; property: "rollAngle"; from: 30; to: -30; duration: 1000; easing.type: QQ2.Easing.OutQuart }
        QQ2.PauseAnimation { duration: 1500 }
        QQ2.NumberAnimation { target: toyplaneTransform; property: "rollAngle"; from: -30; to: 0; duration: 750; easing.type: QQ2.Easing.OutQuart }
        QQ2.PauseAnimation { duration: 2000 }
    }

    components: [
        toyplaneMesh,
        toyplaneTransform,
        material
    ]
}
