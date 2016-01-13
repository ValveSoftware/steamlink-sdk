/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the documentation of the Qt Toolkit.
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

//![0]
import QtQuick 2.3

Item {

    width: 320
    height: 480

    Rectangle {
        color: "#272822"
        width: 320
        height: 480
    }

    Column {
        //![states]

        Item {
            id: container
            width: 320
            height: 120

            Rectangle {
                id: rect
                color: "red"
                width: 120
                height: 120

                MouseArea {
                    anchors.fill: parent
                    onClicked: container.state == 'other' ? container.state = '' : container.state = 'other'
                }
            }
            states: [
                // This adds a second state to the container where the rectangle is farther to the right

                State { name: "other"

                    PropertyChanges {
                        target: rect
                        x: 200
                    }
                }
            ]
            transitions: [
                // This adds a transition that defaults to applying to all state changes

                Transition {

                    // This applies a default NumberAnimation to any changes a state change makes to x or y properties
                    NumberAnimation { properties: "x,y" }
                }
            ]
        }
        //![states]
        //![behave]
        Item {
            width: 320
            height: 120

            Rectangle {
                color: "green"
                width: 120
                height: 120

                // This is the behavior, and it applies a NumberAnimation to any attempt to set the x property
                Behavior on x {

                    NumberAnimation {
                        //This specifies how long the animation takes
                        duration: 600
                        //This selects an easing curve to interpolate with, the default is Easing.Linear
                        easing.type: Easing.OutBounce
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: parent.x == 0 ? parent.x = 200 : parent.x = 0
                }
            }
        }
        //![behave]
        //![constant]
        Item {
            width: 320
            height: 120

            Rectangle {
                color: "blue"
                width: 120
                height: 120

                // By setting this SequentialAnimation on x, it and animations within it will automatically animate
                // the x property of this element
                SequentialAnimation on x {
                    id: xAnim
                    // Animations on properties start running by default
                    running: false
                    loops: Animation.Infinite // The animation is set to loop indefinitely
                    NumberAnimation { from: 0; to: 200; duration: 500; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 200; to: 0; duration: 500; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 250 } // This puts a bit of time between the loop
                }

                MouseArea {
                    anchors.fill: parent
                    // The animation starts running when you click within the rectangle
                    onClicked: xAnim.running = true
                }
            }
        }
        //![constant]

        //![scripted]
        Item {
            width: 320
            height: 120

            Rectangle {
                id: rectangle
                color: "yellow"
                width: 120
                height: 120

                MouseArea {
                    anchors.fill: parent
                    // The animation starts running when you click within the rectange
                    onClicked: anim.running = true;
                }
            }

            // This animation specifically targets the Rectangle's properties to animate
            SequentialAnimation {
                id: anim
                // Animations on their own are not running by default
                // The default number of loops is one, restart the animation to see it again

                NumberAnimation { target: rectangle; property: "x"; from: 0; to: 200; duration: 500 }

                NumberAnimation { target: rectangle; property: "x"; from: 200; to: 0; duration: 500 }
            }
        }
        //![scripted]
    }
}
//![0]
