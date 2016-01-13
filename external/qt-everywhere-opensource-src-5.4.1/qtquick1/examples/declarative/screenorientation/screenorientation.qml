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
import "Core"
import "Core/screenorientation.js" as ScreenOrientation

Rectangle {
    id: window
    width: 360
    height: 640
    color: "white"

    Rectangle {
        id: main
        clip: true
        property variant selectedOrientation: Orientation.UnknownOrientation
        property variant activeOrientation: selectedOrientation == Orientation.UnknownOrientation ? runtime.orientation : selectedOrientation
        state: "orientation " + activeOrientation
        property bool inPortrait: (activeOrientation == Orientation.Portrait || activeOrientation == Orientation.PortraitInverted);

        // rotation correction for landscape devices like N900
        property bool landscapeWindow: window.width > window.height
        property variant rotationDelta: landscapeWindow ? -90 : 0
        rotation: rotationDelta

        // initial state is portrait
        property real baseWidth: landscapeWindow ? window.height-10 : window.width-10
        property real baseHeight: landscapeWindow ? window.width-10 : window.height-10

        width: baseWidth
        height: baseHeight
        anchors.centerIn: parent

        color: "black"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0.5,0.5,0.5,0.5) }
            GradientStop { position: 0.8; color: "black" }
            GradientStop { position: 1.0; color: "black" }
        }
        Item {
            id: bubbles
            property bool rising: false
            anchors.fill: parent
            property variant gravityPoint: ScreenOrientation.calculateGravityPoint(main.activeOrientation, runtime.orientation)
            Repeater {
                model: 24
                Bubble {
                    rising: bubbles.rising
                    verticalRise: ScreenOrientation.parallel(main.activeOrientation, runtime.orientation)
                    xAttractor: parent.gravityPoint.x
                    yAttractor: parent.gravityPoint.y
                }
            }
            Component.onCompleted: bubbles.rising = true;
        }

        Column {
            width: centeredText.width
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenterOffset: 30
            Text {
                text: "Orientation"
                color: "white"
                font.pixelSize: 22
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                id: centeredText
                text: ScreenOrientation.printOrientation(main.activeOrientation)
                color: "white"
                font.pixelSize: 40
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: "sensor: " + ScreenOrientation.printOrientation(runtime.orientation)
                color: "white"
                font.pixelSize: 14
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        Flow {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            spacing: 4
            Button {
                width: main.inPortrait ? (parent.width-4)/2 : (parent.width-8)/3
                text: "Portrait"
                onClicked: main.selectedOrientation = Orientation.Portrait
                toggled: main.selectedOrientation == Orientation.Portrait
            }
            Button {
                width: main.inPortrait ? (parent.width-4)/2 : (parent.width-8)/3
                text: "Portrait inverted"
                onClicked: main.selectedOrientation = Orientation.PortraitInverted
                toggled: main.selectedOrientation == Orientation.PortraitInverted
            }
            Button {
                width: main.inPortrait ? (parent.width-4)/2 : (parent.width-8)/3
                text: "Landscape"
                onClicked: main.selectedOrientation = Orientation.Landscape
                toggled: main.selectedOrientation == Orientation.Landscape
            }
            Button {
                width: main.inPortrait ? (parent.width-4)/2 : (parent.width-8)/3
                text: "Landscape inverted"
                onClicked: main.selectedOrientation = Orientation.LandscapeInverted
                toggled: main.selectedOrientation == Orientation.LandscapeInverted
            }
            Button {
                width: main.inPortrait ? parent.width : 2*(parent.width-2)/3
                text: "From runtime.orientation"
                onClicked: main.selectedOrientation = Orientation.UnknownOrientation
                toggled: main.selectedOrientation == Orientation.UnknownOrientation
            }
        }
        states: [
            State {
                name: "orientation " + Orientation.Landscape
                PropertyChanges {
                    target: main
                    rotation: ScreenOrientation.getAngle(Orientation.Landscape)+rotationDelta
                    width: baseHeight
                    height: baseWidth
                }
            },
            State {
                name: "orientation " + Orientation.PortraitInverted
                PropertyChanges {
                    target: main
                    rotation: ScreenOrientation.getAngle(Orientation.PortraitInverted)+rotationDelta
                    width: baseWidth
                    height: baseHeight
                }
            },
            State {
                name: "orientation " + Orientation.LandscapeInverted
                PropertyChanges {
                    target: main
                    rotation: ScreenOrientation.getAngle(Orientation.LandscapeInverted)+rotationDelta
                    width: baseHeight
                    height: baseWidth
                }
            }
        ]
        transitions: Transition {
            ParallelAnimation {
                RotationAnimation {
                    direction: RotationAnimation.Shortest
                    duration: 300
                    easing.type: Easing.InOutQuint
                }
                NumberAnimation {
                    properties: "x,y,width,height"
                    duration: 300
                    easing.type: Easing.InOutQuint
                }
            }
        }
    }
}
