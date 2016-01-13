/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
//! [0]
import QtSensors 5.0
//! [0]
import QtMultimedia 5.0



Rectangle {
    id: window
    width: 320
    height: 480

    state: "default"

    Audio {
        id :phone
        source: "audio/phone.wav" //mono
    }
    Audio {
        id :loopy2a_mono
        source: "audio/loopy2a_mono.wav" //mono
    }

    Text {
        id: label
        text: qsTr("Shake to rotate triangles")
        y: parent.height / 4
        anchors.horizontalCenter: parent.horizontalCenter
    }
    Image {
        id: triangle1
        smooth: true
        source: "content/triangle.png"
        x: parent.width / 2 - (triangle1.width / 2)
        y: parent.height / 2 - (triangle1.height);
        Behavior on x { SmoothedAnimation { velocity: 200 } }
        Behavior on y { SmoothedAnimation { velocity: 200 } }
        transform: Rotation {
             id: myRot
        }
    }
    Image {
        id: triangle2
        smooth: true
        source: "content/triangle2.png"
        x: parent.width / 2 - (triangle1.width + triangle2.width / 2)
        y: parent.height / 2 + (triangle2.height / 2);
        Behavior on x { SmoothedAnimation { velocity: 200 } }
        Behavior on y { SmoothedAnimation { velocity: 200 } }
    }
    Image {
        id: triangle3
        smooth: true
        source: "content/triangle3.png"
        x: parent.width / 2 + (triangle1.width / 2)
        y: parent.height / 2 + (triangle3.height / 2);

        Behavior on x { SmoothedAnimation { velocity: 200 } }
        Behavior on y { SmoothedAnimation { velocity: 200 } }
    }

    states: [
        State {
            name: "rotated"
            PropertyChanges { target: triangle1; rotation: 180 }
            PropertyChanges { target: triangle2; rotation: 90 }
            PropertyChanges { target: triangle3; rotation: 270 }
        },
        State {
            name: "default"
            PropertyChanges { target: triangle1; rotation: 0;
                x: parent.width / 2 - (triangle1.width / 2)
                y: parent.height / 2 - (triangle1.height);
            }
            PropertyChanges { target: triangle2; rotation: 0;
                x: parent.width / 2 - (triangle1.width + triangle2.width / 2)
                y: parent.height / 2 + (triangle2.height / 2);
            }
            PropertyChanges { target: triangle3; rotation: 0;
                x: parent.width / 2 + (triangle1.width / 2)
                y: parent.height / 2 + (triangle3.height / 2);
            }
        },
        State {
            name: "whipped"
            PropertyChanges { target: triangle1; rotation: 0; x:0; }
            PropertyChanges { target: triangle2; rotation: 0; x:0; y:triangle1.x + triangle1.height; }
            PropertyChanges { target: triangle3; rotation: 0; x:0;
                y: triangle2.y + triangle2.height; }
        },
        State {
            name: "twistedR"
            PropertyChanges { target: triangle1; rotation: 270;
                x:window.width - triangle1.width;
            }
            PropertyChanges { target: triangle2; rotation: 180;
                x:window.width - triangle2.width;
            }
            PropertyChanges { target: triangle3; rotation: 90;
                x:window.width - triangle3.width;
                y:triangle2.y + triangle2.height;
            }
        },
        State {
            name: "twistedL"
            PropertyChanges { target: triangle1; rotation: 270;
                x:0;
            }
            PropertyChanges { target: triangle2; rotation: 180;
                x:0;
            }
            PropertyChanges { target: triangle3; rotation: 90;
                x:0;
                y:triangle2.y + triangle2.height;
            }
        },
        State {
            name: "covered"
            PropertyChanges { target: triangle1; rotation: 0;
                x: window.width / 3 - triangle1.width / 2;
                y: window.height - triangle1.height;
            }
            PropertyChanges { target: triangle2; rotation: 0;
                x: window.width / 2 - triangle2.width / 2; // middle
                y: window.height - triangle2.height;
            }
            PropertyChanges { target: triangle3; rotation: 0;
                x: (window.width / 3 + window.width / 3) - triangle3.width / 2;
                y: window.height - triangle3.height;
            }
        },
        State {
            name: "hovered"
            PropertyChanges { target: triangle1; rotation: 90;
                x: window.width / 3 - triangle1.width / 2;
                y: triangle1.height;
            }
            PropertyChanges { target: triangle2; rotation: 270;
                x: window.width / 2 - triangle2.width / 2; // middle
                y: triangle2.height;
            }
            PropertyChanges { target: triangle3; rotation: 195;
                x: (window.width / 3 + window.width / 3) - triangle3.width / 2;
                y: triangle3.height
            }
        },
        State {
            name :"slammed"
            PropertyChanges { target: triangle1; rotation: 0;
                x: 0;
                y: 0 + 30
            }
            PropertyChanges { target: triangle2; rotation: 0;
                x: window.width - triangle2.width;
                y: 0 + 30
                ;}
            PropertyChanges { target: triangle3; rotation: 0;
                x: window.width / 2 - triangle3.width / 2;
                y:window.height - triangle3.height;
            }
        },
        State {
            name: "doubletapped"
            PropertyChanges { target: triangle1; rotation: 114;
                 transformOrigin: Item.BottomLeft
            }
            PropertyChanges { target: triangle2; rotation: 120;
                transformOrigin: Item.BottomLeft
            }
        }
    ]


    transitions: [
        Transition {

        ParallelAnimation {
            NumberAnimation { properties: "x,y"; easing.type: Easing.OutBounce;duration: 2000; }
            RotationAnimation { id: t1Rotation; target: triangle1; duration: 1000;
                direction: RotationAnimation.Clockwise }
            RotationAnimation { id: t2Rotation; target: triangle2; duration: 2000;
                direction: RotationAnimation.Counterclockwise }
            RotationAnimation { id: t3Rotation; target: triangle3; duration: 2000;
                direction: RotationAnimation.Clockwise }
        }

    }, Transition {
            to: "slammed"
            SequentialAnimation {
                NumberAnimation { properties: "x"; easing.type: Easing.OutBounce;duration: 500; }

            }
        }, Transition {
            to: "doubletapped"
            SequentialAnimation {
                PropertyAction { target: triangle1; property: "transformOrigin" }
                PropertyAction { target: triangle2; property: "transformOrigin" }
                NumberAnimation {  target: triangle1; properties: "rotation"; easing.type: Easing.OutBounce;duration: 500; }
                NumberAnimation {  target: triangle2; properties: "rotation"; easing.type: Easing.OutBounce;duration: 1500; }
            }
        }, Transition {
            from: "doubletapped"
            SequentialAnimation {
                NumberAnimation { properties: "rotation"; easing.type: Easing.OutBounce;duration: 1500; }
                PropertyAction { target: triangle1; property: "transformOrigin" }
                PropertyAction { target: triangle2; property: "transformOrigin" }
            }
        }
    ]

//! [1]
    SensorGesture {
//! [1]
        id: sensorGesture
//! [3]
        enabled: true
//! [3]
//! [2]
        gestures : ["QtSensors.shake", "QtSensors.whip", "QtSensors.twist", "QtSensors.cover",
            "QtSensors.hover", "QtSensors.turnover", "QtSensors.pickup", "QtSensors.slam" , "QtSensors.doubletap"]
//! [2]
//! [4]
        onDetected:{
            console.debug(gesture)
            label.text = gesture

            if (gesture == "shake") {
                window.state == "rotated" ? window.state = "default" : window.state = "rotated"
                timer.start()
            }
            if (gesture == "whip") {
                window.state == "whipped" ? window.state = "default" : window.state = "whipped"
                timer.start()
            }
            if (gesture == "twistRight") {
                window.state == "twistedR" ? window.state = "default" : window.state = "twistedR"
                timer.start()
            }
            if (gesture == "twistLeft") {
                window.state == "twistedL" ? window.state = "default" : window.state = "twistedL"
                timer.start()
            }
            if (gesture == "cover") {
                window.state == "covered" ? window.state = "default" : window.state = "covered"
                timer.start()
            }
            if (gesture == "hover") {
                window.state == "hovered" ? window.state = "default" : window.state = "hovered"
                timer.start()
            }
            if (gesture == "turnover") {
                window.state = "default"
                loopy2a_mono.play();
                timer.start()
            }
            if (gesture == "pickup") {
                window.state = "default"
                phone.play()
                timer.start()
            }
            if (gesture == "slam") {
                window.state == "slammed" ? window.state = "default" : window.state = "slammed"
                timer.start()
            }
            if (gesture == "doubletap") {
                window.state == "doubletapped" ? window.state = "default" : window.state = "doubletapped"
                timer.start()
            }
        }
//! [4]
    }
    Timer {
        id: timer
        running: false
        repeat: false
        interval: 3000
        onTriggered: {
            console.log("timer triggered")
            window.state = "default"
            label.text = "Try another gesture"
        }
    }
}
