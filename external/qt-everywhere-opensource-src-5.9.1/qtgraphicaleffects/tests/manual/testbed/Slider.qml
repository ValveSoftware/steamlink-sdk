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

    property real value: 1
    property real maximum: 1
    property real minimum: 0
    property string caption: ""
    property bool pressed: mouseArea.pressed
    property bool integer: false
    property string handleSource: "images/slider_handle.png"

    width: parent.width
    height: 20

    function updatePos() {
        if (maximum > minimum) {
            var pos = (track.width - 10) * (value - minimum) / (maximum - minimum) + 5;
            return Math.min(Math.max(pos, 5), track.width - 5) - 10;
        } else {
            return 5;
        }
    }

    Text {
        id: captionText
        width: 110
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        text: slider.caption + ':'
        font.family: "Arial"
        font.pixelSize: 11
        color: "#B3B3B3"
    }

    Text {
        id: valueCaption
        anchors.left: captionText.right
        anchors.leftMargin: 11
        anchors.verticalCenter: parent.verticalCenter
        width: 35
        horizontalAlignment: Text.AlignLeft
        text: integer ? slider.value.toFixed(0) : slider.value.toFixed(1)
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
    }

    Item {
        id: track
        height: parent.height
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: parent.width / 2
        anchors.right: parent.right
        anchors.rightMargin: 10

        BorderImage {
            source: "images/slider_track.png"
            anchors.left: parent.left
            anchors.right: parent.right
            border.right: 2
        }

        BorderImage {
            id: trackFilled
            anchors.left: minimum == -maximum ? (value < 0 ? handle.horizontalCenter : parent.horizontalCenter) : parent.left
            anchors.right: minimum == -maximum && value < 0 ? parent.horizontalCenter : handle.horizontalCenter
            source: "images/slider_track_filled.png"
            border.left: 3
            border.right: 3
        }

        Image {
            id: handle;
            smooth: true
            source: mouseArea.pressed ? 'images/slider_handle_pressed.png' : slider.handleSource
            x: updatePos()
        }

        MouseArea {
            id: mouseArea
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            height: 16
            preventStealing: true

            onPressed: {
                var handleX = Math.max(0, Math.min(mouseX, mouseArea.width))
                var realValue = (maximum - minimum) * handleX / mouseArea.width + minimum;
                value = slider.integer ? Math.round(realValue) : realValue;
            }

            onPositionChanged: {
                if (pressed) {
                    var handleX = Math.max(0, Math.min(mouseX, mouseArea.width))
                    var realValue = (maximum - minimum) * handleX / mouseArea.width + minimum;
                    value = slider.integer ? Math.round(realValue) : realValue;
                }
            }
        }
    }
}
