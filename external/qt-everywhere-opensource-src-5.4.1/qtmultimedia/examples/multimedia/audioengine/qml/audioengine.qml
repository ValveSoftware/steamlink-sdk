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

import QtAudioEngine 1.0
import QtQuick 2.0
import "content"

Rectangle {
    color:"white"
    id: root
    width: 300
    height: 500
    property int radius : 100
    property real twoPi : Math.PI + Math.PI

    MyAudioEngine {
        id:audioEngine
        listener.position : Qt.vector3d(observer.x, observer.y, 0);
    }

    Item {
        id: circle
        x: root.width / 2
        y: root.height / 2
        property real percent: 0
        SequentialAnimation on percent {
            id: circleAnim1
            loops: Animation.Infinite
            running: true
            NumberAnimation {
            duration: 8000
            from: 0
            to: 1
            }

        }
    }

    Item {
        id: holder
        x: circle.x - Math.sin(circle.percent * root.twoPi) * root.radius - 50
        y: circle.y + Math.cos(circle.percent * root.twoPi) * root.radius + 50
    }

    Rectangle {
        color:"green"
        id: observer
        width: 16
        height: 16
        x: circle.x - width / 2
        y: circle.y - height / 2
    }
    Rectangle {
        color:"red"
        id: starship
        width: 32
        height: 32
        x: holder.x - width / 2
        y: holder.y - height / 2
    }
    MouseArea {
       anchors.fill: parent
       onClicked: {
           audioEngine.sounds["effects"].play(Qt.vector3d(holder.x, holder.y, 0));
       }
    }

    SoundInstance {
        id: shipSound
        engine:audioEngine
        sound:"shipengine"
        position: Qt.vector3d(holder.x, holder.y, 0)
        direction: {
            var a = (starship.rotation / 360) * root.twoPi;
            return Qt.vector3d(Math.sin(a), -Math.cos(a), 0);
        }
        velocity: {
            var speed = root.twoPi * root.radius / 4;
            return shipSound.direction * speed;
        }

        Component.onCompleted: shipSound.play()
   }

    //Category Volume Control
    Rectangle {
        property variant volumeCtrl: audioEngine.categories["sfx"];
        id: volumeBar
        x: 10
        y: 10
        width: 280
        height: 22
        color: "darkgray"
        Rectangle {
            id: volumeTracker
            x: 0
            y: 0
            width: volumeBar.volumeCtrl.volume * parent.width;
            height: parent.height
            color: "lightgreen"
        }
        Text {
            text: " volume:" + volumeBar.volumeCtrl.volume * 100 +"%";
            font.pointSize: 16;
            font.italic: true;
            color: "black"
            anchors.fill: parent
        }

        MouseArea {
            anchors.fill: parent
            property bool m:false
            onPressed: {
                m = true;
                updateVolume(mouse);
            }
            onReleased: {
                m = false;
            }

            onPositionChanged: {
                if (m) {
                    updateVolume(mouse);
                }
            }
            function updateVolume(mouse) {
                volumeBar.volumeCtrl.volume = Math.min(1, Math.max(0, mouse.x / (volumeBar.width - 1)));
            }
        }
    }

    //Information display
    Item {
        x:10
        y:32
        Text {
            text: " [live instances] = " + audioEngine.liveInstances;
            font.pointSize: 14;
            font.italic: true;
            color: "black"
            anchors.fill: parent
        }
    }

    Item {
        x:10
        y:60
        Text {
            text: " [loading]=" + (audioEngine.loading ? "true" : "false");
            font.pointSize: 16;
            font.italic: true;
            color: "black"
            anchors.fill: parent
        }
    }
}
