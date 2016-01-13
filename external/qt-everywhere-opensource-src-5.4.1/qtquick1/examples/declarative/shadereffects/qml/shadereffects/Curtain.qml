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
    id: main
    anchors.fill: parent

    Rectangle{
        id: bg
        anchors.fill: parent
        color: "black"
    }

    Image {
        source: "images/qt-logo.png"
        anchors.centerIn: parent
    }

    Image {
        id: fabric
        anchors.fill: parent
        source: "images/fabric.jpg"
        fillMode: Image.Tile
    }

    CurtainEffect {
        id: curtain
        anchors.fill: fabric
        bottomWidth: topWidth
        source: ShaderEffectSource { sourceItem: fabric; hideSource: true }

        Behavior on bottomWidth {
            SpringAnimation { easing.type: Easing.OutElastic; velocity: 250; mass: 1.5; spring: 0.5; damping: 0.05}
        }

        SequentialAnimation on topWidth {
            id: topWidthAnim
            loops: Animation.Infinite

            NumberAnimation { to: 360; duration: 1000 }
            PauseAnimation { duration: 2000 }
            NumberAnimation { to: 180; duration: 1000 }
            PauseAnimation { duration: 2000 }
        }
    }

    MouseArea {
        anchors.fill: parent

        onPressed: {
            topWidthAnim.stop()
            curtain.topWidth = mouseX;
        }

        onReleased: {
            topWidthAnim.restart()
        }

        onPositionChanged: {
            if (pressed) {
                curtain.topWidth = mouseX;
            }
        }
    }
}
