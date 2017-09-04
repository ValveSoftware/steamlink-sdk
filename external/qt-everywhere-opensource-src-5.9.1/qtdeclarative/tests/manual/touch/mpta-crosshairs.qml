/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the manual tests of the Qt Toolkit.
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

import QtQuick 2.9
import QtQuick.Window 2.2

Rectangle {
    id: root
    width: 480
    height: 480
    color: "black"

    MultiPointTouchArea {
        id: mpta
        anchors.fill: parent
        touchPoints: [
            TouchPoint { property color color: "red" },
            TouchPoint { property color color: "orange" },
            TouchPoint { property color color: "lightsteelblue" },
            TouchPoint { property color color: "green" },
            TouchPoint { property color color: "blue" },
            TouchPoint { property color color: "violet" },
            TouchPoint { property color color: "steelblue" },
            TouchPoint { property color color: "magenta" },
            TouchPoint { property color color: "goldenrod" },
            TouchPoint { property color color: "darkgray" }
        ] }

    Repeater {
        model: 10

        Item {
            id: crosshairs
            property TouchPoint touchPoint
            x: touchPoint.x - width / 2
            y: touchPoint.y - height / 2
            width: 300; height: 300
            visible: touchPoint.pressed
            rotation: touchPoint.rotation

            Rectangle {
                color: touchPoint.color
                anchors.centerIn: parent
                width: 2; height: parent.height
                antialiasing: true
            }
            Rectangle {
                color: touchPoint.color
                anchors.centerIn: parent
                width: parent.width; height: 2
                antialiasing: true
            }
            Rectangle {
                color: touchPoint.color
                implicitWidth: label.implicitWidth + 8
                implicitHeight: label.implicitHeight + 16
                radius: width / 2
                anchors.centerIn: parent
                antialiasing: true
                Rectangle {
                    color: "black"
                    opacity: 0.35
                    width: (parent.width - 8) * touchPoint.pressure
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    antialiasing: true
                }
                Rectangle {
                    color: "transparent"
                    border.color: "white"
                    border.width: 2
                    opacity: 0.75
                    visible: width > 0
                    width: touchPoint.ellipseDiameters.width
                    height: touchPoint.ellipseDiameters.height
                    radius: Math.min(width, height) / 2
                    anchors.centerIn: parent
                    antialiasing: true
                }
                Text {
                    id: label
                    anchors.centerIn: parent
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    property string uid: touchPoint.uniqueId === undefined || touchPoint.uniqueId.numericId === -1 ?
                                             "" : "\nUID " + touchPoint.uniqueId.numericId
                    text: "x " + touchPoint.x.toFixed(1) +
                          "\ny " + touchPoint.y.toFixed(1) + uid +
                          "\nID " + touchPoint.pointId.toString(16) +
                          "\n∡" + touchPoint.rotation.toFixed(1) + "°"
                }
            }
            Rectangle {
                id: velocityVector
                visible: width > 0
                width: touchPoint.velocity.length()
                height: 4
                Behavior on width { SmoothedAnimation { duration: 200 } }
                radius: height / 2
                antialiasing: true
                color: "gray"
                x: crosshairs.width / 2
                y: crosshairs.height / 2
                rotation: width > 0 ? Math.atan2(touchPoint.velocity.y, touchPoint.velocity.x) * 180 / Math.PI - crosshairs.rotation : 0
                Behavior on rotation { SmoothedAnimation { duration: 20 } }
                transformOrigin: Item.BottomLeft
            }

            Component.onCompleted: touchPoint = mpta.touchPoints[index]
        }
    }
}
