/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

import QtQuick 2.0
import QtQuick.Particles 2.0
Item {
    id: root
    width: 450; height: 600

    property int currentSet: 0
    property bool useStock: false

    Component {
        id: viewDelegate

        Rectangle {
            id: item
            property bool movn: false
            property int parts: 0

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
            }
            Emitter {
                id: emitter2
                system: ps
                anchors.fill: parent
                enabled: item.movn
                emitRate: parts
                velocity: AngleDirection {
                    angle: 0
                    angleVariation: 360
                    magnitude: 2
                    magnitudeVariation: 5
                }
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
            color: "red"
        }
    }


    /* States (Selected operation Transitions) */
    states: [
        State {
            name: "setA"
            when: currentSet == 0
            PropertyChanges {
                target: listview
                add: a_add
                move: a_move
                remove: a_remove
                displaced: a_displaced
            }
        },
        State {
            name: "setB"
            when: currentSet == 1
            PropertyChanges {
                target: listview
                add: b_add
                move: b_move
                remove: b_remove
                displaced: b_displaced
            }
        },
        State {
            name: "setC"
            when: currentSet == 2
            PropertyChanges {
                target: listview
                add: c_add
                move: c_move
                remove: c_remove
                addDisplaced: c_addDisplaced
                moveDisplaced: c_moveDisplaced
                removeDisplaced: c_removeDisplaced
            }
        },
        State {
            name: "setD"
            when: currentSet == 3
            PropertyChanges {
                target: listview
                move: d_move
                moveDisplaced: d_moveDisplaced
            }
            PropertyChanges {
                target: root
                useStock: true
            }
        }
    ]

    /* Transitions */
    Transition {
        id: a_add
        ParallelAnimation {
            NumberAnimation { target: a_add.ViewTransition.item; properties: "opacity"; from: 0; to: 1; duration: 1000 }
        }
    }
    Transition {
        id: a_remove
        SequentialAnimation {
            NumberAnimation { target: a_remove.ViewTransition.item; properties: "opacity"; from: 1; to: 0; duration: 1000 }
        }
    }
    Transition {
        id: a_move
        ColorAnimation { target: a_move.ViewTransition.item; properties: "color"; to: "lightsteelblue"; duration: 1000 }
        PathAnimation {
            duration: 1000
            target: a_move.ViewTransition.item
            path: Path {
                PathCurve { x: a_move.ViewTransition.destination.x + 150; y: a_move.ViewTransition.destination.y + 100 }
                PathCurve { x: a_move.ViewTransition.destination.x + 150; y: a_move.ViewTransition.destination.y }
                PathCurve { x: a_move.ViewTransition.destination.x; y: a_move.ViewTransition.destination.y }
            }
        }
    }
    Transition {
        id: a_displaced
        PropertyAction { target: a_displaced.ViewTransition.item; property: "color"; value: "lightsteelblue" }
        ParallelAnimation {
            NumberAnimation { target: a_displaced.ViewTransition.item; property: "opacity"; to: 1; duration: 250 }
            SequentialAnimation {
                PauseAnimation { duration: a_displaced.ViewTransition.index * 200 }
                NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutQuad }
            }
        }
    }


    Transition {
        id: b_add
        SequentialAnimation {
            NumberAnimation { target: b_add.ViewTransition.item; properties: "scale"; from: 1; to: 1.3; duration: 100 }
            NumberAnimation { target: b_add.ViewTransition.item; properties: "scale"; from: 1.3; to: 1; duration: 500; easing.type: Easing.OutBounce }
        }
    }
    Transition {
        id: b_move
        SequentialAnimation {
            PauseAnimation { duration: b_move.ViewTransition.index * 100 }
            ParallelAnimation {
                ColorAnimation { target: b_move.ViewTransition.item; properties: "color"; from: "red"; to: "lightsteelblue"; duration: 2000 }
                SequentialAnimation {
                    PathAnimation {
                        duration: 1000
                        target: b_move.ViewTransition.item
                        path: Path {
                            PathCurve { x: b_move.ViewTransition.destination.x + 150; y: b_move.ViewTransition.destination.y + 100 }
                            PathCurve { relativeX: 30; relativeY: -100 }
                        }
                    }
                    NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutBounce }
                }
            }
        }
    }
    Transition {
        id: b_remove
        SequentialAnimation {
            NumberAnimation { target: b_remove.ViewTransition.item; properties: "opacity"; from: 1; to: 0; duration: 1000 }
        }
    }
    Transition {
        id: b_displaced
        PropertyAction { target: b_displaced.ViewTransition.item; property: "color"; value: "lightsteelblue" }
        SequentialAnimation {
            PauseAnimation { duration: b_displaced.ViewTransition.item.x == 0 ? b_displaced.ViewTransition.index * 200 : 0 }
            NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutBounce }
        }
    }


    Transition {
        id: c_add
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 400 }
        NumberAnimation { property: "scale"; from: 0; to: 1.0; duration: 400 }
        property int xoff
        xoff: c_add.ViewTransition.index % 2 == 0 ? 200 : -200
        PathAnimation {
            duration: 1000
            path: Path {
                startX: c_add.ViewTransition.destination.x + c_add.xoff
                startY: c_add.ViewTransition.destination.y + 200
                PathCurve { relativeX: -100; relativeY: -50 }
                PathCurve { relativeX: 50; relativeY: -150 }
                PathCurve {
                    x: c_add.ViewTransition.destination.x
                    y: c_add.ViewTransition.destination.y
                }
            }
        }
    }
    Transition {
        id: c_move
        ColorAnimation { target: c_move.ViewTransition.item; properties: "color"; from: "red"; to: "lightsteelblue"; duration: 1000 }
        PathAnimation {
            duration: 1000
            target: c_move.ViewTransition.item
            path: Path {
                PathCurve { x: c_move.ViewTransition.destination.x + 150; y: c_move.ViewTransition.destination.y + 100 }
                PathCurve { x: c_move.ViewTransition.destination.x + 150; y: c_move.ViewTransition.destination.y }
                PathCurve { x: c_move.ViewTransition.destination.x; y: c_move.ViewTransition.destination.y }
            }
        }
    }
    Transition {
        id: c_remove
        PropertyAnimation { target: c_remove.ViewTransition.item; properties: "opacity"; to: 0; duration: 3000; easing.type: Easing.OutInBounce }
    }
    Transition {
        id: c_addDisplaced

        PropertyAction { property: "color"; value: "lightsteelblue" }
        PropertyAction { property: "opacity"; value: 1.0 }
        PropertyAction { property: "scale"; value: 1.0 }
        SequentialAnimation {
            PauseAnimation { duration: c_addDisplaced.ViewTransition.index * 200 }
            NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutElastic }
        }

    }
    Transition {
        id: c_moveDisplaced

        PropertyAction { property: "color"; value: "lightsteelblue" }
        PropertyAction { property: "opacity"; value: 1.0 }
        PropertyAction { property: "scale"; value: 1.0 }
        SequentialAnimation {
            NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutBounce }
        }

    }
    Transition {
        id: c_removeDisplaced

        PropertyAction { property: "color"; value: "lightsteelblue" }
        PropertyAction { property: "opacity"; value: 1.0 }
        PropertyAction { property: "scale"; value: 1.0 }
        SequentialAnimation {
            PauseAnimation { duration: (c_removeDisplaced.ViewTransition.index * 200) + 3000 }
            NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutElastic }
        }

    }
    Transition {
        id: d_move
        SequentialAnimation {
            PropertyAction { target: d_move.ViewTransition.item; property: "movn"; value: true }
            ParallelAnimation {
                NumberAnimation { target: d_move.ViewTransition.item; properties: "opacity"; to: 0; duration: 2000 }
                NumberAnimation { target: d_move.ViewTransition.item; properties: "parts"; to: 500; duration: 2000 }
            }
            NumberAnimation { properties: "x,y"; duration: 1000*(d_move.ViewTransition.index+1) }
            ParallelAnimation {
                NumberAnimation { target: d_move.ViewTransition.item; properties: "opacity"; to: 1; duration: 2000 }
                NumberAnimation { target: d_move.ViewTransition.item; properties: "parts"; to: 0; duration: 2000 }
            }
            PropertyAction { target: d_move.ViewTransition.item; property: "movn"; value: false }
        }
    }

    Transition {
        id: d_moveDisplaced
        PropertyAction { target: d_moveDisplaced.ViewTransition.item; property: "color"; value: "lightsteelblue" }
        SequentialAnimation {
            PauseAnimation { duration: 500*(d_moveDisplaced.ViewTransition.index+1) }
            NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutQuad }
        }
    }

    /* Buttons */
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
            Text { anchors.centerIn: parent; text: "To Top" }
            MouseArea { anchors.fill: parent; onClicked: listview.model.move(listview.currentIndex, 0, 1) }
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
            Text { anchors.centerIn: parent; text: "Add" }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.model.insert(listview.currentIndex+1, {"name": "New item", "original": false } )
            }
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
            Text { anchors.centerIn: parent; text: "Remove" }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.model.remove(listview.currentIndex)
            }
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
            Text { anchors.centerIn: parent; text: "Append" }
            MouseArea {
                anchors.fill: parent
                onClicked: listview.model.append({"name": "New item", "original": false })
            }
        }
    }
    Column {
        spacing: 2
        anchors.right: parent.right
        anchors.rightMargin: 2
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: currentSet == 0 ? "green" : "lightgray"
                    ColorAnimation on color { duration: 1000 }
                }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "Set A" }
            MouseArea { anchors.fill: parent; onClicked: { currentSet = 0 } }
        }
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: currentSet == 1 ? "green" : "lightgray" }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "Set B" }
            MouseArea { anchors.fill: parent; onClicked: { currentSet = 1 } }
        }
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: currentSet == 2 ? "green" : "lightgray" }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "Set C" }
            MouseArea { anchors.fill: parent; onClicked: { currentSet = 2 } }
        }
        Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "darkgray" }
                GradientStop { position: 0.5; color: currentSet == 3 ? "green" : "lightgray" }
                GradientStop { position: 1.0; color: "darkgray" }
            }
            radius: 6
            border.color: "black"
            height: 50; width: 80
            Text { anchors.centerIn: parent; text: "Set D" }
            MouseArea { anchors.fill: parent; onClicked: { currentSet = 3 } }
        }
    }
}
