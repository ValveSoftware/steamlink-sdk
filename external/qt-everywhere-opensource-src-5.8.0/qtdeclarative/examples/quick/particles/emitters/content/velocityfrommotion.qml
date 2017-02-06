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

    height: 540
    width: 360

    gradient: Gradient {
        GradientStop { position: 0; color: "#000020" }
        GradientStop { position: 1; color: "#000000" }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: root
    }

    ParticleSystem { id: sys1 }
    ImageParticle {
        system: sys1
        source: "qrc:///particleresources/glowdot.png"
        color: "cyan"
        alpha: 0
        SequentialAnimation on color {
            loops: Animation.Infinite
            ColorAnimation {
                from: "cyan"
                to: "magenta"
                duration: 1000
            }
            ColorAnimation {
                from: "magenta"
                to: "blue"
                duration: 2000
            }
            ColorAnimation {
                from: "blue"
                to: "violet"
                duration: 2000
            }
            ColorAnimation {
                from: "violet"
                to: "cyan"
                duration: 2000
            }
        }
        colorVariation: 0.3
    }
    //! [0]
    Emitter {
        id: trailsNormal
        system: sys1

        emitRate: 500
        lifeSpan: 2000

        y: mouseArea.pressed ? mouseArea.mouseY : circle.cy
        x: mouseArea.pressed ? mouseArea.mouseX : circle.cx

        velocity: PointDirection {xVariation: 4; yVariation: 4;}
        acceleration: PointDirection {xVariation: 10; yVariation: 10;}
        velocityFromMovement: 8

        size: 8
        sizeVariation: 4
    }
    //! [0]
    ParticleSystem { id: sys2 }
    ImageParticle {
        color: "cyan"
        system: sys2
        alpha: 0
        SequentialAnimation on color {
            loops: Animation.Infinite
            ColorAnimation {
                from: "magenta"
                to: "cyan"
                duration: 1000
            }
            ColorAnimation {
                from: "cyan"
                to: "magenta"
                duration: 2000
            }
        }
        colorVariation: 0.5
        source: "qrc:///particleresources/star.png"
    }
    Emitter {
        id: trailsStars
        system: sys2

        emitRate: 100
        lifeSpan: 2200


        y: mouseArea.pressed ? mouseArea.mouseY : circle.cy
        x: mouseArea.pressed ? mouseArea.mouseX : circle.cx

        velocity: PointDirection {xVariation: 4; yVariation: 4;}
        acceleration: PointDirection {xVariation: 10; yVariation: 10;}
        velocityFromMovement: 8

        size: 22
        sizeVariation: 4
    }
    ParticleSystem { id: sys3; }
    ImageParticle {
        source: "qrc:///particleresources/glowdot.png"
        system: sys3
        color: "orange"
        alpha: 0
        SequentialAnimation on color {
            loops: Animation.Infinite
            ColorAnimation {
                from: "red"
                to: "green"
                duration: 2000
            }
            ColorAnimation {
                from: "green"
                to: "red"
                duration: 2000
            }
        }

        colorVariation: 0.2

    }
    Emitter {
        id: trailsNormal2
        system: sys3

        emitRate: 300
        lifeSpan: 2000

        y: mouseArea.pressed ? mouseArea.mouseY : circle2.cy
        x: mouseArea.pressed ? mouseArea.mouseX : circle2.cx

        velocityFromMovement: 16

        velocity: PointDirection {xVariation: 4; yVariation: 4;}
        acceleration: PointDirection {xVariation: 10; yVariation: 10;}

        size: 12
        sizeVariation: 4
    }
    ParticleSystem { id: sys4; }
    ImageParticle {
        system: sys4
        source: "qrc:///particleresources/star.png"
        color: "green"
        alpha: 0
        SequentialAnimation on color {
            loops: Animation.Infinite
            ColorAnimation {
                from: "green"
                to: "red"
                duration: 2000
            }
            ColorAnimation {
                from: "red"
                to: "green"
                duration: 2000
            }
        }

        colorVariation: 0.5
    }
    Emitter {
        id: trailsStars2
        system: sys4

        emitRate: 50
        lifeSpan: 2200


        y: mouseArea.pressed ? mouseArea.mouseY : circle2.cy
        x: mouseArea.pressed ? mouseArea.mouseX : circle2.cx

        velocityFromMovement: 16
        velocity: PointDirection {xVariation: 2; yVariation: 2;}
        acceleration: PointDirection {xVariation: 10; yVariation: 10;}

        size: 22
        sizeVariation: 4
    }



    color: "white"

    Item {
        id: circle
        //anchors.fill: parent
        property real radius: 0
        property real dx: root.width / 2
        property real dy: root.height / 2
        property real cx: radius * Math.sin(percent*6.283185307179) + dx
        property real cy: radius * Math.cos(percent*6.283185307179) + dy
        property real percent: 0

        SequentialAnimation on percent {
            loops: Animation.Infinite
            running: true
            NumberAnimation {
            duration: 1000
            from: 1
            to: 0
            loops: 8
            }
            NumberAnimation {
            duration: 1000
            from: 0
            to: 1
            loops: 8
            }

        }

        SequentialAnimation on radius {
            loops: Animation.Infinite
            running: true
            NumberAnimation {
                duration: 4000
                from: 0
                to: 100
            }
            NumberAnimation {
                duration: 4000
                from: 100
                to: 0
            }
        }
    }

    Item {
        id: circle3
        property real radius: 100
        property real dx: root.width / 2
        property real dy: root.height / 2
        property real cx: radius * Math.sin(percent*6.283185307179) + dx
        property real cy: radius * Math.cos(percent*6.283185307179) + dy
        property real percent: 0

        SequentialAnimation on percent {
            loops: Animation.Infinite
            running: true
            NumberAnimation { from: 0.0; to: 1 ; duration: 10000;  }
        }
    }

    Item {
        id: circle2
        property real radius: 30
        property real dx: circle3.cx
        property real dy: circle3.cy
        property real cx: radius * Math.sin(percent*6.283185307179) + dx
        property real cy: radius * Math.cos(percent*6.283185307179) + dy
        property real percent: 0

        SequentialAnimation on percent {
            loops: Animation.Infinite
            running: true
            NumberAnimation { from: 0.0; to: 1 ; duration: 1000; }
        }
    }

}
