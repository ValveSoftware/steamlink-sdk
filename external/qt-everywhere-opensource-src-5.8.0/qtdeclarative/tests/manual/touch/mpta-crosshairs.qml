/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.4
import QtQuick.Window 2.2

Rectangle {
    id: root
    color: "black"

    MultiPointTouchArea {
        id: mpta
        anchors.fill: parent
        touchPoints: [
            TouchPoint { property color color: "red" },
            TouchPoint { property color color: "orange" },
            TouchPoint { property color color: "yellow" },
            TouchPoint { property color color: "green" },
            TouchPoint { property color color: "blue" },
            TouchPoint { property color color: "violet" },
            TouchPoint { property color color: "cyan" },
            TouchPoint { property color color: "magenta" },
            TouchPoint { property color color: "goldenrod" },
            TouchPoint { property color color: "darkgray" }
        ] }

    Repeater {
        model: 10

        Item {
            anchors.fill: parent
            property TouchPoint touchPoint
            visible: touchPoint.pressed

            Rectangle {
                color: touchPoint.color
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 2
                x: touchPoint.x - 1
            }
            Rectangle {
                color: touchPoint.color
                anchors.left: parent.left
                anchors.right: parent.right
                height: 2
                y: touchPoint.y - 1
            }
            Rectangle {
                color: touchPoint.color
                width: 50 * touchPoint.pressure
                height: width
                radius: width / 2
                x: touchPoint.x - width / 2
                y: touchPoint.y - width / 2
            }
            Rectangle {
                id: velocityVector
                visible: width > 0
                width: touchPoint.velocity.length()
                height: 1
                x: touchPoint.x
                y: touchPoint.y
                rotation: width > 0 ? Math.acos(touchPoint.velocity.x / width) : 0
                transformOrigin: Item.BottomLeft
            }

            Component.onCompleted: touchPoint = mpta.touchPoints[index]
        }
    }
}
