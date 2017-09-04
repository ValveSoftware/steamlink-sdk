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
    property bool integer: false

    width: parent.width
    height: 20

    function updatePos() {
        if (maximum > minimum) {
            var pos = (track.width) * (value - minimum) / (maximum - minimum);
            return Math.min(Math.max(pos, 0), track.width);
        } else {
            return 0;
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
        text: slider.value.toFixed(1)
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
            anchors.left: parent.left
            width: updatePos()
            source: "images/slider_track_filled.png"
            border.left: 3
            border.right: 3
        }
    }
}
