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

import QtQuick 2.0
import QtQuick.Scene3D 2.0
import Qt3D.Render 2.0
import QtQuick.Controls 1.4


Item {

    Rectangle {
        id: scene
        property bool colorChange: true
        anchors.fill: parent
        color: "#2d2d2d"

        transform: Rotation {
            id: sceneRotation
            axis.x: 1
            axis.y: 0
            axis.z: 0
            origin.x: scene.width / 2
            origin.y: scene.height / 2
        }
        Rectangle {
            id: controlsbg
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.topMargin: 10
            anchors.rightMargin: 1720
            anchors.bottomMargin: 10
            color: "grey"
            Column {
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.topMargin: 5
                spacing: 10
                Rectangle {
                    id: slidertexscale
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: scaletext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "TEXTURE SCALE"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider1
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 1.0
                        minimumValue: 0.3
                    }
                }
                Rectangle {
                    id: slidertexturespeed
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: texturespeedtext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "TEXTURE SPEED"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider5
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 1.1
                        maximumValue: 4.0
                        minimumValue: 0.0
                    }
                }
                Rectangle {
                    id: sliderspecularity
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: specularitytext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "SPECULARITY"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider3
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 1.0
                        maximumValue: 3.0
                        minimumValue: 0.0
                    }
                }
                Rectangle {
                    id: sliderdistortion
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: distortiontext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "DISTORTION"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider7
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 0.015
                        maximumValue: 0.1
                        minimumValue: 0.0
                    }
                }
                Rectangle {
                    id: slidernormal
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: normaltext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "NORMAL AMOUNT"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider8
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 2.2
                        maximumValue: 4.0
                        minimumValue: 0.0
                    }
                }
                Rectangle {
                    id: sliderwavespeed
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: wawespeedtext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "WAVE SPEED"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider2
                        updateValueWhileDragging: false
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 0.75
                        maximumValue: 4.0
                        minimumValue: 0.1
                    }
                }
                Rectangle {
                    id: sliderwaveheight
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: waweheighttext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "WAVE HEIGHT"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider6
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 0.2
                        maximumValue: 0.5
                        minimumValue: 0.02
                    }
                }
                Rectangle {
                    id: slidermeshrotation
                    width: 180
                    height: 60
                    color: "#2d2d2d"
                    Text {
                        id: meshrotationtext
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: "MESH ROTATION"
                        color: "white"
                        font.bold: true
                        font.pointSize: 12
                    }
                    Slider {
                        id: slider4
                        anchors.fill: parent
                        anchors.topMargin: 30
                        anchors.rightMargin: 10
                        anchors.leftMargin: 10
                        value: 35.0
                        maximumValue: 360.0
                        minimumValue: 0.0
                    }
                }
            }
        }

        Scene3D {
            id: scene3d
            anchors.fill: parent
            anchors.leftMargin: 200
            anchors.topMargin: 10
            anchors.rightMargin: 10
            anchors.bottomMargin: 10
            focus: true
            aspects: ["input", "logic"]
            cameraAspectRatioMode: Scene3D.AutomaticAspectRatio

            SceneRoot {
                id: root
            }
        }
    }
}
