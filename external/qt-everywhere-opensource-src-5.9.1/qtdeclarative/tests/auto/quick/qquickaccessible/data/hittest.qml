/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
import "widgets"

Rectangle {
    id: page
    width: 640
    height: 480
    color: "white"
    Accessible.name: "root"
    Accessible.role: Accessible.Client
    Rectangle {
        id: header
        color: "#c0c0c0"
        height: usage.height + chkClip.height
        anchors.left: parent.left
        anchors.right: parent.right
        Text {
            id: usage
            text: "Use an a11y inspect tool to see if all visible rectangles can be found with hit testing."
        }
        Rectangle {
            id: chkClip
            property bool checked: true

            color: (checked ? "#f0f0f0" : "#c0c0c0")
            height: label.height
            width: label.width
            anchors.left: parent.left
            anchors.bottom: parent.bottom

            MouseArea {
                anchors.fill: parent
                onClicked: chkClip.checked = !chkClip.checked
            }
            Text {
                id: label
                text: "Click here to toggle clipping"
            }
        }
    }
    TextRect {
        clip: chkClip.checked
        z: 2
        id: rect1
        text: "rect1"
        width: 100
        height: 100
        color: "#ffc0c0"
        anchors.top: header.bottom
        TextRect {
            id: rect10
            text: "rect10"
            width: 100
            height: 100
            x: 50
            y: 50
            color: "#ffa0a0"
            TextRect {
                id: rect100
                text: "rect100"
                width: 100
                height: 100
                x: 80
                y: 80
                color: "#ff8080"
            }
            TextRect {
                id: rect101
                text: "rect101"
                x: 100
                y: 70
                z: 3
                width: 100
                height: 100
                color: "#e06060"
            }
            TextRect {
                id: rect102
                text: "rect102"
                width: 100
                height: 100
                x: 150
                y: 60
                color: "#c04040"
            }
        }
    }

    TextRect {
        x: 0
        y: 50
        id: rect2
        text: "rect2"
        width: 100
        height: 100
        color: "#c0c0ff"
        TextRect {
            id: rect20
            text: "rect20"
            width: 100
            height: 100
            x: 50
            y: 50
            color: "#a0a0ff"
            TextRect {
                id: rect200
                text: "rect200"
                width: 100
                height: 100
                x: 80
                y: 80
                color: "#8080ff"
            }
            TextRect {
                id: rect201
                text: "rect201"
                x: 100
                y: 70
                z: 100
                width: 100
                height: 100
                color: "#6060e0"
            }
            TextRect {
                id: rect202
                text: "rect202"
                width: 100
                height: 100
                x: 150
                y: 60
                color: "#4040c0"
            }
        }
    }

    TextRect {
        x: 0
        y: 300
        text: "rect3"
        width: 200
        height: 100
        color: "#ffa0a0"
        TextRect {
            x: 10
            y: 10
            text: "rect30"
            width: 80
            height: 80
            color: "#ffa0a0"
            TextRect {
                x: 30
                y: 50
                text: "rect300"
                width: 80
                height: 80
                color: "#ffa0a0"
            }
        }
        TextRect {
            x: 100
            y: 10
            text: "rect31"
            width: 80
            height: 80
            color: "#ffa0a0"
            TextRect {
                x: -50
                y: 60
                z: -3
                text: "rect310"
                width: 80
                height: 80
                color: "#ffa0a0"
            }
        }
    }
}
