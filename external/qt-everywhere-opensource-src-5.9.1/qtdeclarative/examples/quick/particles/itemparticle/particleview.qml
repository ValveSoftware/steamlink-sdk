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
import "content/script.js" as Script
import "content"

Item {
    id: root
    width: 640
    height: 480
    Rectangle {
        anchors.fill: parent
        color: "black"
        z: -1
    }
    Item {
        id: loading
        Behavior on opacity {NumberAnimation {}}
        anchors.fill: parent
        Text {
            anchors.centerIn: parent
            text: "Loading"
            color: "white"
        }
    }
    ParticleSystem {
        id: sys;
        running: true
    }
    Emitter {
        id: emitter
        system: sys
        height: parent.height - 132/2
        x: -132/2
        y: 132/2
        velocity: PointDirection { x: 32; xVariation: 8 }
        emitRate: 0.5
        lifeSpan: Emitter.InfiniteLife
        group: "photos"
    }
    Age {
        system: sys
        x: parent.width + 132/2
        height: parent.height
        width: 1000
    }
    ImageParticle {
        system: sys
        groups: ["fireworks"]
        source: "qrc:particleresources/star.png"
        color: "lightsteelblue"
        alpha: 0
        colorVariation: 0
        z: 1000
    }
    ItemParticle {
        id: mp
        z: 0
        system: sys
        fade: false
        groups: ["photos"]
    }
    Component {
        id: alertDelegate
        Rectangle {
            width: 132
            height: 132
            NumberAnimation on scale {
                running: true
                loops: 1
                from: 0.2
                to: 1
            }
            Image {
                source: "../asteroid/../images/rocket.png"
                anchors.centerIn: parent
            }
            Text {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: "A new ship has arrived!"
            }
        }
    }
    property Item alertItem;
    function alert() {
        force.enabled = true;
        alertItem = alertDelegate.createObject(root);
        alertItem.x = root.width/2 - alertItem.width/2
        alertItem.y = root.height/2 - alertItem.height/2
        spawnFireworks.pulse(200);
        stopAlert.start();
    }
    focus: true
    Keys.onSpacePressed: alert();
    Timer {
        id: stopAlert
        running: false
        repeat: false
        interval: 800
        onTriggered: {
            force.enabled = false;
            mp.take(alertItem, true);
            centerEmitter.burst(1);
        }
    }
    Attractor {
        id: force
        system: sys
        pointX: root.width/2
        pointY: root.height/2
        strength: -10000
        enabled: false
        anchors.centerIn: parent
        width: parent.width/2
        height: parent.height/2
        groups:["photos"]
        affectedParameter: Attractor.Position
    }
    Emitter {
        id: centerEmitter
        velocity: PointDirection { x: 32; xVariation: 8;}
        emitRate: 0.5
        lifeSpan: 12000 //TODO: A -1 or something which does 'infinite'? (but need disable fade first)
        maximumEmitted: 20
        group: "photos"
        system: sys
        anchors.centerIn: parent
        enabled: false

        //TODO: Zoom in effect
    }
    Emitter {
        id: spawnFireworks
        group: "fireworks"
        system: sys
        maximumEmitted: 400
        emitRate: 400
        lifeSpan: 2800
        x: parent.width/2
        y: parent.height/2 - 64
        width: 8
        height: 8
        enabled: false
        size: 32
        endSize: 8
        velocity: AngleDirection { magnitude: 160; magnitudeVariation: 120; angleVariation: 90; angle: 270 }
        acceleration: PointDirection { y: 160 }
    }
    Item { x: -1000; y: -1000 //offscreen
        Repeater {//Load them here, add to system on completed
            model: theModel
            delegate: theDelegate
        }
    }
    RssModel {id: theModel; tags:"particle,particles"}
    Component {
        id: theDelegate
        Rectangle {
            id: container
            border.width: 2
            antialiasing: true
            property real myRand: Math.random();//'depth'
            z: Math.floor(myRand * 100)
            scale: (myRand + 1.0)/2;
            //TODO: Darken based on 'depth'
            width: 132
            height: 132
            //ItemParticle.onAttached: console.log("I'm in"  + x + "," + y + ":" + opacity);
            ItemParticle.onDetached: mp.take(container);//respawns
            function manage()
            {
                if (state == "selected") {
                    console.log("Taking " + index);
                    mp.freeze(container);
                } else {
                    console.log("Returning " +index);
                    mp.unfreeze(container);
                }
            }
            Image {
                id: img
                anchors.centerIn: parent
                antialiasing: true;
                source: "http://" + Script.getImagePath(content); cache: true
                fillMode: Image.PreserveAspectFit;
                width: parent.width-4; height: parent.height-4
                onStatusChanged: if (img.status == Image.Ready) {
                    loading.opacity = 0;
                    mp.take(container);
                }
            }
            Text {
                anchors.bottom: parent.bottom
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: title
                color: "black"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: container.state == "selected" ? container.state = "" : container.state = "selected"
            }
            states: State {
                name: "selected"
                ParentChange {
                    target: container
                    parent: root
                    x: 0
                    y: 0
                }
                PropertyChanges {
                    target: container
                    width: root.width
                    height: root.height
                    z: 101
                    opacity: 1
                    rotation: 0
                }
            }
            transitions: Transition {
                to: "selected"
                reversible: true
                SequentialAnimation {
                    ScriptAction {script: container.manage();}
                    ParallelAnimation {
                        ParentAnimation {NumberAnimation { properties: "x,y" }}//Doesn't work, particles takes control of x,y instantly
                        NumberAnimation { properties: "width, height, z, rotation" }
                    }
                }
            }
        }
    }
}
