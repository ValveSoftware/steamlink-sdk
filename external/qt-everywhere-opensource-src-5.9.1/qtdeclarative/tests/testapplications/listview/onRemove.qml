/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
Item {
    id: root
    width: 450; height: 600

    Component {
        id: viewDelegate

        Rectangle {
            id: item
            signal boom
            Connections {
                target: item
                onBoom: emitter.burst(1000)
            }

            width: 225; height: 40
            border.width: ListView.isCurrentItem ? 3 : 1
            z: ListView.isCurrentItem ? 100 : 1
            color: original ? "lightsteelblue" : "yellow"
            objectName: name
            Text { x: 10; text: name; font.pixelSize: 20 }
            MouseArea { anchors.fill: parent; onClicked: listview.currentIndex = index }

            Emitter {
                id: emitter
                system: ps
                anchors.fill: parent
                enabled: false
                velocity: AngleDirection {
                    angle: 0
                    angleVariation: 360
                    magnitude: 50
                    magnitudeVariation: 50
                }
                lifeSpan: 2000
            }

            ListView.onRemove: SequentialAnimation {
                PropertyAction { target: item; property: "ListView.delayRemove"; value: true }
                PropertyAction { target: item; property: "opacity"; value: 0 }
                ScriptAction { script: item.boom() }
                PauseAnimation { duration: 1000 }
                PropertyAction { target: item; property: "ListView.delayRemove"; value: false }
            }
        }
    }


    ListView {
        id: listview
        width: 225; height: 500
        anchors.centerIn: parent
        delegate: viewDelegate
        header: Rectangle {
            height: 50; width: 225
            color: "blue"
            Text { anchors.centerIn: parent; text: "Transitions!"; color: "goldenrod" }
        }
        model: ListModel {
            id: a_model
            ListElement { name: "Item A"; original: true }
            ListElement { name: "Item B"; original: true }
            ListElement { name: "Item C"; original: true }
            ListElement { name: "Item D"; original: true }
            ListElement { name: "Item E"; original: true }
            ListElement { name: "Item F"; original: true }
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: "black"
        }

    }

    ParticleSystem {
        id: ps
        ImageParticle {
            id: imageparticle
            source: "star.png"
            color: "blue"
        }
    }
    Column {
        spacing: 2
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: "lightgray" }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 25; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: listview.model.insert(listview.currentIndex+1, {"name": "New item", "original": false } ) }
        }
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: "lightgray" }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "-"; font.pixelSize: 25; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: listview.model.remove(listview.currentIndex) }
        }
    }
}



