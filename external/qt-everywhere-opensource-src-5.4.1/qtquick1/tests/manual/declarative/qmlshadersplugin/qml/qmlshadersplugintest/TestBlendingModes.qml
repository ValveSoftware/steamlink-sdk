/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 1.0
import Qt.labs.shaders 1.0

Item {
    id: blendModeTest
    property real blendItemHeight: 60

    anchors.fill: parent;

    Rectangle {
        width: parent.width / 6
        height: parent.height
        color: "black"
    }
    Rectangle {
        x: parent.width/6 * 1
        width: parent.width / 6
        height: parent.height
        color: "white"
    }
    Rectangle {
        x: parent.width/6 * 2
        width: parent.width / 6
        height: parent.height
        color: "gray"
    }
    Rectangle {
        x: parent.width/6 * 3
        width: parent.width / 6
        height: parent.height
        color: "red"
    }
    Rectangle {
        x: parent.width/6 * 4
        width: parent.width / 6
        height: parent.height
        color: "green"
    }
    Rectangle {
        x: parent.width/6 * 5
        width: parent.width / 6
        height: parent.height
        color: "blue"
    }


    Image {
        anchors.fill: parent;
        source: "image.png"

    }

    Rectangle {
        id: first
        anchors.top: parent.top
        anchors.topMargin: 60
        width: parent.width
        height: blendModeTest.blendItemHeight
        color: "#8000ff00"
        Text {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            text: " Rectangle color #8000ff00"
            color: "white"
        }
    }
    Rectangle {
        id: second
        anchors.top: first.bottom
        anchors.topMargin: 5
        width: parent.width
        height: blendModeTest.blendItemHeight
        color: "#ff00ff00"
        opacity: 0.5
        Text {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            text: " Rectangle color #ff00ff00, opacity 0.5"
            color: "white"
        }
    }

    ShaderEffectItem {
        id: effect
        anchors.top: second.bottom
        anchors.topMargin: 5
        width: parent.width
        height: blendModeTest.blendItemHeight
        fragmentShader: "
        varying highp vec2 qt_TexCoord0;
        void main() {
            gl_FragColor = vec4(0.0, 1.0, 0.0, 0.5);
        }
        "
        Text {
             anchors.bottom: parent.bottom
             anchors.bottomMargin: 5
             text: " ShaderEffectItem gl_FragColor=vec4(0.0, 1.0, 0.0, 0.5)"
             color: "white"
        }
    }

        ShaderEffectItem {
            id: effect2
            anchors.top: effect.bottom
            anchors.topMargin: 5
            width: parent.width
            height: blendModeTest.blendItemHeight
            fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            void main() {
                gl_FragColor = vec4(0.0, 0.5, 0.0, 0.5);
            }
                "
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    text: " ShaderEffectItem gl_FragColor=vec4(0.0, 0.5, 0.0, 0.5)"
                    color: "white"
                }
        }


     Image {
         id: image1
         source: "green_image_transparent.png"
         anchors.top: effect2.bottom
         anchors.topMargin: 5
         width: parent.width
         height: blendModeTest.blendItemHeight
     }
     Text {
         anchors.bottom: image1.bottom
         anchors.bottomMargin: 5
         text: " Image, green and 50% alpha"
         color: "white"
     }


     ShaderEffectItem {
         id: effect3
         property variant source: ShaderEffectSource {
             sourceItem: image1
             hideSource: false
         }
         anchors.top: image1.bottom
         anchors.topMargin: 5

         width: parent.width
         height: blendModeTest.blendItemHeight
         Text {
             anchors.bottom: parent.bottom
             anchors.bottomMargin: 5
             text: " ShaderEffectItem, source item green 50% alpha."
             color: "white"
         }
     }

     ShaderEffectItem {
         id: effect4
         property variant source: ShaderEffectSource {
             sourceItem: Image { source: "green_image_transparent.png" }
             hideSource: true
         }
         anchors.top: effect3.bottom
         anchors.topMargin: 5
         width: parent.width
         height: blendModeTest.blendItemHeight
         Text {
             anchors.bottom: parent.bottom
             anchors.bottomMargin: 5
             text: " ShaderEffectItem, source image green 50% alpha."
             color: "white"
         }
     }


     Rectangle {
         id: greenRect2
         anchors.top: effect4.bottom
         anchors.topMargin: 5
         width: parent.width
         height: blendModeTest.blendItemHeight
         opacity: 0.5
         color: "green"
     }


     ShaderEffectItem {
         id: effect5
         property variant source: ShaderEffectSource { sourceItem: greenRect2; hideSource: true }
         anchors.top: effect4.bottom
         anchors.topMargin: 5

         width: parent.width
         height: blendModeTest.blendItemHeight
         Text {
             anchors.bottom: parent.bottom
             anchors.bottomMargin: 5
             text: " ShaderEffectItem, source item green rect with 0.5 opacity."
             color: "white"
         }
     }

    Timer {
        running: true
        interval: 2000
        repeat: true
        onTriggered: {
        }
    }


    Rectangle {
        width: parent.width
        height: 40
        color: "#cc000000"

        Text {
             id: label
             anchors.centerIn:  parent
             text: "Blending test"
             color: "white"
             font.bold: true
        }
    }
}
