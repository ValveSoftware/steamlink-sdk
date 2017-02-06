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
import Qt3D.Extras 2.0

Entity {
    id: root
    property real hue: 0.0
    property alias animateColors: hueAnim.running
    property Layer layer: null

    QQ2.NumberAnimation {
        id: hueAnim
        target: root
        property: "hue"
        from: 0.0; to: 1.0
        duration: 200000
        running: false
        loops: QQ2.Animation.Infinite
    }

    Entity {
        id: _private
        property color color1: Qt.hsla( (hue + 0.59) % 1, 0.53, 0.59 )
        property color color2: Qt.hsla( (hue + 0.59) % 1, 1.0, 0.15 )
    }

    components: [ layer, mesh, transform, material ]

    PlaneMesh {
        id: mesh
        width: 2.0
        height: 2.0
        meshResolution: Qt.size( 2, 2 )
    }

    Transform {
        id: transform
        // Rotate the plane so that it faces us
        rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 90)
    }

    Material {
        id: material
        effect: BackgroundEffect {}
        parameters: [
            Parameter { name: "color1"; value: Qt.vector3d( _private.color1.r, _private.color1.g, _private.color1.b ) },
            Parameter { name: "color2"; value: Qt.vector3d( _private.color2.r, _private.color2.g, _private.color2.b ) }
        ]
    }
}
