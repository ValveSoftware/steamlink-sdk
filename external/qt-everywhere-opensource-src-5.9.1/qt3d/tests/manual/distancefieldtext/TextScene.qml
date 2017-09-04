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

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.9
import QtQuick 2.0 as QQ2;

Entity {
    id: sceneRoot

    property alias fontFamily : text.font.family
    property alias fontBold : text.font.bold
    property alias fontItalic : text.font.italic

    signal clicked()

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 20.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    FirstPersonCameraController {
        camera: camera
        linearSpeed: 0.1 + camera.position.z
    }

    components: [
        RenderSettings {
            activeFrameGraph : ForwardRenderer {
                camera: camera
                clearColor: "black"
            }
        },
        InputSettings {
        }
    ]

    function getChars(n) {
        var s = "";
        for (var i = 0; i < n; i++) {
            s += String.fromCharCode(32+i);
            if (i % 16 == 15)
                s += "\n";
        }
        return s;
    }

    property int strLen : 28

    onClicked: {
        strLen += 1
        if (strLen > 96)
            strLen = 10
    }

    Entity {
        components: [
            Transform {
                id: rot
                translation: Qt.vector3d(-5, -5, 0)
            }
        ]

        QQ2.SequentialAnimation {
            loops: QQ2.Animation.Infinite
            running: true
            QQ2.NumberAnimation {
                target: rot
                property: "rotationY"
                from: 0; to: -80;
                duration: 2000
            }
            QQ2.NumberAnimation {
                target: rot
                property: "rotationY"
                from: -80; to: 80;
                duration: 4000
            }
            QQ2.NumberAnimation {
                target: rot
                property: "rotationY"
                from: 80; to: 0;
                duration: 2000
            }
        }

        Text2DEntity {
            id: text
            font.pointSize: 1
            text: getChars(strLen)
            width: 20
            height: 10
        }
    }
}
