/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.0
import QtQuick.Particles 2.0

Rectangle {
    id: root
    color: "black"
    width: 640
    height: 480
    ParticleSystem {
        id: sys
    }

    ImageParticle {
        system: sys
        source: "qrc:///particleresources/glowdot.png"
        color: "white"
        colorVariation: 1.0
        alpha: 0.1
        entryEffect: ImageParticle.None
    }

    Emitter {
        id: emitter
        system: sys
        width: parent.width/2
        velocity: PointDirection {y: 72; yVariation: 24}
        lifeSpan: 10000
        emitRate: 1000
        enabled: false
        size: 32
    }

    //! [fake]
    Item {
        id: fakeEmitter
        function burst(number) {
            while (number > 0) {
                var item = fakeParticle.createObject(root);
                item.lifeSpan = Math.random() * 5000 + 5000;
                item.x = Math.random() * (root.width/2) + (root.width/2);
                item.y = 0;
                number--;
            }
        }

        Component {
            id: fakeParticle
            Image {
                id: container
                property int lifeSpan: 10000
                width: 32
                height: 32
                source: "qrc:///particleresources/glowdot.png"
                y: 0
                PropertyAnimation on y {from: -16; to: root.height-16; duration: container.lifeSpan; running: true}
                SequentialAnimation on opacity {
                    running: true
                    NumberAnimation { from:0; to: 1; duration: 500}
                    PauseAnimation { duration: container.lifeSpan - 1000}
                    NumberAnimation { from:1; to: 0; duration: 500}
                    ScriptAction { script: container.destroy(); }
                }
            }
        }
    }
    //! [fake]

    //Hooked to a timer, but click for extra bursts that really stress performance
    Timer {
        interval: 10000
        triggeredOnStart: true
        repeat: true
        running: true
        onTriggered: {
            emitter.burst(1000);
            fakeEmitter.burst(1000);
        }
    }
    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        text: "1000 particles"
        color: "white"
        MouseArea {
            anchors.fill: parent
            onClicked: emitter.burst(1000);
        }
    }
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: "1000 items"
        color: "white"
        MouseArea {
            anchors.fill: parent
            onClicked: fakeEmitter.burst(1000);
        }
    }
}
