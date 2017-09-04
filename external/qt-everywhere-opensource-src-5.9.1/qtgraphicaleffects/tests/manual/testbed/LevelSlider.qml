/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
