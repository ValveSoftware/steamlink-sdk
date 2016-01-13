/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

Item {
    id: slider
    height: 26
    width: 320

    property real min: 0
    property real max: 1
    property real value: min + (max - min) * mousearea.value
    property real init: min+(max-min)/2
    property string name: "Slider"
    property color color: "#0066cc"

    Component.onCompleted: setValue(init)
    function setValue(v) {
       if (min < max)
          handle.x = Math.round( v / (max - min) *
                                (mousearea.drag.maximumX - mousearea.drag.minimumX)
                                + mousearea.drag.minimumX);
    }
    Rectangle {
        id:sliderName
        anchors.left: parent.left
        anchors.leftMargin: 16
        height: childrenRect.height
        width: Math.max(44, childrenRect.width)
        anchors.verticalCenter: parent.verticalCenter
        Text {
            text: slider.name + ":"
            font.pointSize: 12
            color: "#333"
        }
    }

    Rectangle{
        id: foo
        width: parent.width - 8 - sliderName.width
        color: "#eee"
        height: 7
        radius: 3
        antialiasing: true
        border.color: Qt.darker(color, 1.2)
        anchors.left: sliderName.right
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            height: parent.height
            anchors.left: parent.left
            anchors.right: handle.horizontalCenter
            color: slider.color
            radius: 3
            border.width: 1
            border.color: Qt.darker(color, 1.3)
            opacity: 0.8
        }
        Image {
            id: handle
            source: "images/slider_handle.png"
            anchors.verticalCenter: parent.verticalCenter
            MouseArea {
                id: mousearea
                anchors.fill: parent
                anchors.margins: -4
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: Math.round(-handle.width / 2 + 3)
                drag.maximumX: Math.round(foo.width - handle.width/2 - 3)
                property real value: (handle.x - drag.minimumX) / (drag.maximumX - drag.minimumX)
            }
        }
    }
}
