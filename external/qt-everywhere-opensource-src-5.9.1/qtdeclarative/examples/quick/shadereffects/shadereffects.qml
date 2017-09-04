/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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
import QtQml.Models 2.1
import "content"

Rectangle {
    id: root
    width: 320
    height: 480
    property color col: "lightsteelblue"
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.tint(root.col, "#20FFFFFF") }
        GradientStop { position: 0.1; color: Qt.tint(root.col, "#20AAAAAA") }
        GradientStop { position: 0.9; color: Qt.tint(root.col, "#20666666") }
        GradientStop { position: 1.0; color: Qt.tint(root.col, "#20000000") }
    }

    //! [source]
    ShaderEffectSource {
        id: theSource
        sourceItem: theItem
    }
    //! [source]

    function saturate(x) {
        return Math.min(Math.max(x, 0), 1)
    }

    function sliderToColor(x) {
        return Qt.rgba(saturate(Math.max(2 - 6 * x, 6 * x - 4)),
                        saturate(Math.min(6 * x, 4 - 6 * x)),
                        saturate(Math.min(6 * x - 2, 6 - 6 * x)))
    }

    Grid {
        anchors.centerIn: parent
        columns: 2

        Item {
            id: theItem
            width: 160
            height: 160
            ListView {
                anchors.centerIn: parent
                width: 160
                height: 140
                clip: true
                snapMode: ListView.SnapOneItem
                model: ObjectModel {
                    Text {
                        width: 160
                        height: 140
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 118
                        font.family: "Times"
                        color: "blue"
                        text: "Qt"
                    }
                    Image {
                        width: 160
                        height: 140
                        source: "content/qt-logo.png"
                    }
                    Image {
                        width: 160
                        height: 140
                        source: "content/face-smile.png"
                    }
                }
            }
        }
        ShaderEffect {
            width: 160
            height: 160
            property variant source: theSource
            property real amplitude: 0.04 * wobbleSlider.value
            property real frequency: 20
            property real time: 0
            NumberAnimation on time { loops: Animation.Infinite; from: 0; to: Math.PI * 2; duration: 600 }
            //! [fragment]
            fragmentShader: "qrc:shadereffects/content/shaders/wobble.frag"
            //! [fragment]
            Slider {
                id: wobbleSlider
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 40
            }
        }
        ShaderEffect {
            width: 160
            height: 160
            property variant source: theSource
            property variant shadow: ShaderEffectSource {
                sourceItem: ShaderEffect {
                    width: theItem.width
                    height: theItem.height
                    property variant delta: Qt.size(0.0, 1.0 / height)
                    property variant source: ShaderEffectSource {
                        sourceItem: ShaderEffect {
                            width: theItem.width
                            height: theItem.height
                            property variant delta: Qt.size(1.0 / width, 0.0)
                            property variant source: theSource
                            fragmentShader: "qrc:shadereffects/content/shaders/blur.frag"
                        }
                    }
                    fragmentShader: "qrc:shadereffects/content/shaders/blur.frag"
                }
            }
            property real angle: 0
            property variant offset: Qt.point(15.0 * Math.cos(angle), 15.0 * Math.sin(angle))
            NumberAnimation on angle { loops: Animation.Infinite; from: 0; to: Math.PI * 2; duration: 6000 }
            property variant delta: Qt.size(offset.x / width, offset.y / height)
            property real darkness: shadowSlider.value
            fragmentShader: "qrc:shadereffects/content/shaders/shadow.frag"
            Slider {
                id: shadowSlider
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 40
            }
        }
        ShaderEffect {
            width: 160
            height: 160
            property variant source: theSource
            property variant delta: Qt.size(0.5 / width, 0.5 / height)
            fragmentShader: "qrc:shadereffects/content/shaders/outline.frag"
        }
        ShaderEffect {
            width: 160
            height: 160
            property variant source: theSource
            property color tint: root.sliderToColor(colorizeSlider.value)
            fragmentShader: "qrc:shadereffects/content/shaders/colorize.frag"
            Slider {
                id: colorizeSlider
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 40
            }
        }
        ShaderEffect {
            width: 160
            height: 160
            //! [properties]
            property variant source: theSource
            property real bend: 0
            property real minimize: 0
            property real side: genieSlider.value
            SequentialAnimation on bend {
                loops: Animation.Infinite
                NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 1600 }
                NumberAnimation { to: 0; duration: 700; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 1000 }
            }
            SequentialAnimation on minimize {
                loops: Animation.Infinite
                PauseAnimation { duration: 300 }
                NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 1000 }
                NumberAnimation { to: 0; duration: 700; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 1300 }
            }
            //! [properties]
            //! [vertex]
            mesh: Qt.size(10, 10)
            vertexShader: "qrc:shadereffects/content/shaders/genie.vert"
            //! [vertex]
            Slider {
                id: genieSlider
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 40
            }
        }
    }
}
