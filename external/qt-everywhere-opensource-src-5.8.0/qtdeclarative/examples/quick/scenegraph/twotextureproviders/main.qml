/****************************************************************************
**
** Copyright (C) 2014 Gunnar Sletta <gunnar@sletta.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

import QtQuick 2.2
import SceneGraphRendering 1.0

Item {
    id: root

    // The checkers background
    ShaderEffect {
        anchors.fill: parent

        property real tileSize: 16
        property color color1: Qt.rgba(0.9, 0.9, 0.9, 1);
        property color color2: Qt.rgba(0.8, 0.8, 0.8, 1);

        property size pixelSize: Qt.size(width / tileSize, height / tileSize);

        fragmentShader:
            "
        uniform lowp vec4 color1;
        uniform lowp vec4 color2;
        uniform highp vec2 pixelSize;
        varying highp vec2 qt_TexCoord0;
        void main() {
            highp vec2 tc = sign(sin(3.14152 * qt_TexCoord0 * pixelSize));
            if (tc.x != tc.y)
                gl_FragColor = color1;
            else
                gl_FragColor = color2;
        }
        "
    }

    width: 320
    height: 480

    Item {
        id: box
        width: root.width * 0.9
        height: width

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.9
            height: parent.width * 0.4
            radius: width * 0.1;
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.hsla(0.6, 0.9, 0.9, 1); }
                GradientStop { position: 1; color: Qt.hsla(0.6, 0.6, 0.3, 1); }
            }
            RotationAnimator on rotation { from: 0; to: 360; duration: 10000; loops: Animation.Infinite }
        }

        visible: false
        layer.enabled: true
    }

    Item {
        id: text
        width: box.width
        height: width
        Text {
            anchors.centerIn: parent
            color: "black" // Qt.hsla(0.8, 0.8, 0.8);
            text: "Qt\nQuick"

            horizontalAlignment: Text.AlignHCenter

            font.bold: true
            font.pixelSize: text.width * 0.25
            RotationAnimator on rotation { from: 360; to: 0; duration: 9000; loops: Animation.Infinite }
        }
        visible: false
        layer.enabled: true
    }

    XorBlender {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.height * 0.05;
        width: box.width
        height: box.height
        source1: box
        source2: text
    }

    Rectangle {
        id: labelFrame
        anchors.margins: -10
        radius: 10
        color: "white"
        border.color: "black"
        opacity: 0.8
        anchors.fill: description
    }

    Text {
        id: description
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        wrapMode: Text.WordWrap
        text: "This example creates two animated items and sets 'layer.enabled: true' on both of them. " +
              "This turns the items into texture providers and we can access their texture from C++ in a custom material. " +
              "The XorBlender is a custom C++ item which uses performs an Xor blend between them."
    }
}
