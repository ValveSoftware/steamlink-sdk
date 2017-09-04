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
    id: colorSlider

    property real value: 1
    property real maximum: 1
    property real minimum: 0
    property string caption: ""
    property bool pressed: mouseArea.pressed
    property bool integer: false
    property string handleSource: "images/slider_handle.png"
    property real handleOpacity: 1.0
    property alias trackItem: track.children

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
        text: colorSlider.caption + ':'
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
        text: colorSlider.value.toFixed(1)
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
    }

    Item {
        id: track
        height: 4
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: parent.width / 2
        anchors.right: parent.right
        anchors.rightMargin: 10

        Image {
            id: handle;
            anchors.verticalCenter: parent.verticalCenter
            smooth: true
            source: mouseArea.pressed ? 'images/slider_handle_pressed.png' : colorSlider.handleSource
            opacity: colorSlider.handleOpacity
            x: updatePos()
            z: 1
        }

        MouseArea {
            id: mouseArea
            anchors {left: parent.left; right: parent.right; leftMargin: 5; rightMargin: 5; verticalCenter: parent.verticalCenter}
            height: 8
            preventStealing: true

            onPressed: {
                var handleX = Math.max(0, Math.min(mouseX, mouseArea.width))
                var realValue = (maximum - minimum) * handleX / mouseArea.width + minimum;
                value = colorSlider.integer ? Math.round(realValue) : realValue;
            }

            onPositionChanged: {
                if (pressed) {
                    var handleX = Math.max(0, Math.min(mouseX, mouseArea.width))
                    var realValue = (maximum - minimum) * handleX / mouseArea.width + minimum;
                    value = colorSlider.integer ? Math.round(realValue) : realValue;
                }
            }
        }
    }
}
