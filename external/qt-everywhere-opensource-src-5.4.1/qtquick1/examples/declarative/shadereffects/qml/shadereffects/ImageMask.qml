/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

import QtQuick 1.0
import Qt.labs.shaders 1.0

Item {
    anchors.fill: parent

    Image {
        id: bg
        anchors.fill: parent
        source: "images/image2.jpg"
    }

    Item {
        id: mask
        anchors.fill: parent

        Text {
            text: "Mask text"
            font.pixelSize: 50
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter

            NumberAnimation on rotation {
                running: true
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 3000
            }

            SequentialAnimation on y {
                running: true
                loops: Animation.Infinite
                NumberAnimation {
                    to: main.height
                    duration: 3000
                }
                NumberAnimation {
                    to: 0
                    duration: 3000
                }
            }
        }

        Rectangle {
            id: opaqueBox
            width: 60
            height: parent.height
            SequentialAnimation on x {
                running: true
                loops: Animation.Infinite
                NumberAnimation {
                    to: main.width
                    duration: 2000
                    easing.type: Easing.InOutCubic
                }
                NumberAnimation {
                    to: 0
                    duration: 2000
                    easing.type: Easing.InOutCubic
                }
            }
        }

        Rectangle {
            width: 100
            opacity: 0.5
            height: parent.height
            SequentialAnimation on x {
                PauseAnimation { duration: 100 }

                SequentialAnimation {
                    loops: Animation.Infinite
                    NumberAnimation {
                        to: main.width
                        duration: 2000
                        easing.type: Easing.InOutCubic
                    }
                    NumberAnimation {
                        to: 0
                        duration: 2000
                        easing.type: Easing.InOutCubic
                    }
                }
            }
        }
    }

    ImageMaskEffect {
        anchors.fill: parent
        image: ShaderEffectSource {
            sourceItem: Image { source: "images/image1.jpg" }
            live: false
            hideSource: true
        }
        mask: ShaderEffectSource {
            sourceItem: mask
            live: true
            hideSource: true
        }
    }
}
