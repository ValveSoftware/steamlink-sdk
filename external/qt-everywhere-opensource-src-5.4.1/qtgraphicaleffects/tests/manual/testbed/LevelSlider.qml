/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
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

Item {
    id: slider

    width: parent.width
    height: 30

    property real midPointValue: 0.5
    property real blackPointValue: 0.0
    property real whitePointValue: 1.0
    property real maximum: 1
    property real minimum: 0
    property real gamma: Math.min(10.0, Math.max(0.1, 1/(Math.log(0.5) / Math.log(midPointValue))))
    property string caption: ""
    property bool integer: false
    property bool showMidPoint: true

    Text {
        id: captionText
        width: 110
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 13
        horizontalAlignment: Text.AlignRight
        text: slider.caption + ':'
        font.family: "Arial"
        font.pixelSize: 11
        color: "#B3B3B3"
    }

    Item {
        id: track
        height: parent.height
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: parent.width / 2 - 30
        anchors.right: parent.right
        anchors.rightMargin: 10

        BorderImage {
            id: trackImage
            source: "images/slider_track.png"
            anchors.left: parent.left
            anchors.right: parent.right
            border.right: 2
            width: parent.width
        }

        BorderImage {
            id: trackFilled
            anchors.left: blackpointHandle.x < whitePointHandle.x ? blackpointHandle.right : whitePointHandle.right
            anchors.right: blackpointHandle.x < whitePointHandle.x ? whitePointHandle.left : blackpointHandle.left
            anchors.margins: -10
            source: "images/slider_track_filled.png"
            border.left: 3
            border.right: 3
        }

        Image {
            id: blackpointHandle;
            smooth: true
            source: blackpointMouseArea.pressed ? "images/slider_handle_pressed.png" : "images/slider_handle_black.png"
            x: trackImage.x - width/2 + 5
            width: 20
            onXChanged: {
                blackPointValue = minimum + maximum * ((x + 5) / (track.width - 10))
                midpointHandle.x = blackpointHandle.x + ((whitePointHandle.x - blackpointHandle.x) * midPointValue)
            }
        }

        Image {
            id: midpointHandle;
            smooth: true
            source: midpointMouseArea.pressed ? "images/slider_handle_pressed.png" : "images/slider_handle_gray.png"
            x: blackpointHandle.x + ((whitePointHandle.x - blackpointHandle.x) * 0.5)
            visible: showMidPoint
            width: 20
            onXChanged: {
                if (midpointMouseArea.pressed) {
                    midPointValue = (x - Math.min(whitePointHandle.x, blackpointHandle.x)) /  Math.abs(whitePointHandle.x - blackpointHandle.x)
                }
            }
        }

        Image {
            id: whitePointHandle;
            smooth: true
            source: whitepointMouseArea.pressed ? "images/slider_handle_pressed.png" : "images/slider_handle_white.png"
            x: trackImage.x + trackImage.width - width/2 - 5
            width: 20
            onXChanged: {
                whitePointValue = minimum + maximum * ((x + 5) / (track.width - 10))
                midpointHandle.x = blackpointHandle.x + ((whitePointHandle.x - blackpointHandle.x) * midPointValue)
            }
        }

        MouseArea {
            id: blackpointMouseArea
            anchors.fill: blackpointHandle
            anchors.margins: -5
            drag.target: blackpointHandle
            drag.axis: Drag.XAxis
            drag.minimumX: -5
            drag.maximumX: trackImage.width - blackpointHandle.width + 5
        }

        MouseArea {
            id: whitepointMouseArea
            anchors.fill: whitePointHandle
            anchors.margins: -5
            drag.target: whitePointHandle
            drag.axis: Drag.XAxis
            drag.minimumX: -5
            drag.maximumX: trackImage.width - whitePointHandle.width + 5
        }

        MouseArea {
            id: midpointMouseArea
            anchors.fill: midpointHandle
            anchors.margins: -5
            drag.target: midpointHandle
            drag.axis: Drag.XAxis
            drag.minimumX: Math.min(blackpointHandle.x, whitePointHandle.x)
            drag.maximumX: Math.max(whitePointHandle.x, blackpointHandle.x)
        }
    }

    Text {
        id: blackPointValueCaption
        anchors.bottom: track.bottom
        anchors.left: track.left
        text: slider.blackPointValue.toFixed(1)
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
    }

    Text {
        id: midPointValueCaption
        anchors.bottom: track.bottom
        anchors.left: track.left
        anchors.right: track.right
        horizontalAlignment: Text.AlignHCenter
        width: track.width
        text: slider.gamma.toFixed(1)
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
        visible: showMidPoint
    }

    Text {
        id: whitePointValueCaption
        anchors.bottom: track.bottom
        anchors.right: track.right
        text: slider.whitePointValue.toFixed(1)
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
    }
}
