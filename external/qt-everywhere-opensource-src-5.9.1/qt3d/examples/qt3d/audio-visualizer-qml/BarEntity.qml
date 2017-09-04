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
import Qt3D.Extras 2.0
import QtQuick 2.4 as QQ2

Entity {
    property int rotationTimeMs: 0
    property int entityIndex: 0
    property int entityCount: 0
    property int startAngle: 0 + 360 / entityCount * entityIndex
    property bool needsNewMagnitude: true
    property real magnitude: 0
    property real animWeight: 0

    property color lowColor: "black"
    property color highColor: "#b3b3b3"
    property color barColor: lowColor

    property string entityAnimationsState: "stopped"
    property bool entityAnimationsPlaying: true

    property var entityMesh: null

    onEntityAnimationsStateChanged: {
        if (animationState == "paused") {
            if (angleAnimation.running)
                angleAnimation.pause()
            if (barColorAnimations.running)
                barColorAnimations.pause()
        } else if (animationState == "playing"){
            needsNewMagnitude = true;
            if (heightDecreaseAnimation.running)
                heightDecreaseAnimation.stop()
            if (angleAnimation.paused) {
                angleAnimation.resume()
            } else if (!entityAnimationsPlaying) {
                magnitude = 0
                angleAnimation.start()
                entityAnimationsPlaying = true
            }
            if (barColorAnimations.paused)
                barColorAnimations.resume()
        } else {
            if (animWeight != 0)
                heightDecreaseAnimation.start()
            needsNewMagnitude = true
            angleAnimation.stop()
            barColorAnimations.stop()
            entityAnimationsPlaying = false
        }
    }

    property Material barMaterial: PhongMaterial {
        diffuse: barColor
        ambient: Qt.darker(barColor)
        specular: "black"
        shininess: 1
    }

    property Transform angleTransform: Transform {
        property real heightIncrease: magnitude * animWeight
        property real barAngle: startAngle

        matrix: {
            var m = Qt.matrix4x4()
            m.rotate(barAngle, Qt.vector3d(0, 1, 0))
            m.translate(Qt.vector3d(1.1, heightIncrease / 2 - heightIncrease * 0.05, 0))
            m.scale(Qt.vector3d(0.5, heightIncrease * 15, 0.5))
            return m;
        }

        property real compareAngle: barAngle
        onBarAngleChanged: {
            compareAngle = barAngle

            if (compareAngle > 360)
                compareAngle = barAngle - 360

            if (compareAngle > 180) {
                parent.enabled = false
                animWeight = 0
                if (needsNewMagnitude) {
                    // Calculate the ms offset where the bar will be at the center point of the
                    // visualization and fetch the correct magnitude for that point in time.
                    var offset = (90.0 + 360.0 - compareAngle) * (rotationTimeMs / 360.0)
                    magnitude = mediaPlayer.getNextAudioLevel(offset)
                    needsNewMagnitude = false
                }
            } else {
                parent.enabled = true
                // Calculate a power of 2 curve for the bar animation that peaks at 90 degrees
                animWeight = Math.min((compareAngle / 90), (180 - compareAngle) / 90)
                animWeight = animWeight * animWeight
                if (!needsNewMagnitude) {
                    needsNewMagnitude = true
                    barColorAnimations.start()
                }
            }
        }
    }

    components: [entityMesh, barMaterial, angleTransform]

    //![0]
    QQ2.NumberAnimation {
        id: angleAnimation
        target: angleTransform
        property: "barAngle"
        duration: rotationTimeMs
        loops: QQ2.Animation.Infinite
        running: true
        from: startAngle
        to: 360 + startAngle
    }
    //![0]

    QQ2.NumberAnimation {
        id: heightDecreaseAnimation
        target: angleTransform
        property: "heightIncrease"
        duration: 400
        running: false
        from: angleTransform.heightIncrease
        to: 0
        onStopped: barColor = lowColor
    }

    property int animationDuration: angleAnimation.duration / 6

    //![1]
    QQ2.SequentialAnimation on barColor {
        id: barColorAnimations
        running: false

        QQ2.ColorAnimation {
            from: lowColor
            to: highColor
            duration: animationDuration
        }

        QQ2.PauseAnimation {
            duration: animationDuration
        }

        QQ2.ColorAnimation {
            from: highColor
            to: lowColor
            duration: animationDuration
        }
    }
    //![1]
}
