/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.3

Rectangle {
    width: 1024
    height: 1024
    color: "white"

    TextRectangle {
        id: textRect
    }

    Rectangle {
        id: rect
        x: 0
        y: 0
        width: 200
        height: 200
        color: "red"

        ColorAnimation {
            target: rect
            loops: Animation.Infinite
            property: "color"
            from: "blue"
            to: "green"
            duration: 2000
            running: true
        }
    }

    Rectangle {
        id: rect2
        width: 300
        height: 100
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "red"

        ColorAnimation {
            target: rect2
            loops: Animation.Infinite
            property: "color"
            from: "red"
            to: "white"
            duration: 2000
            running: true
        }
    }

    Button {
        id: button
        width: 100
        height: 80
        anchors.top: rect.bottom
        anchors.left: parent.left
        text: "button"
        scale: 2.0
    }
    CheckBox {
        id: checkbox
        width: 200
        height: 180
        scale: 2.0
        text: "checkbox"
        checked: true
        anchors.top: parent.top
        anchors.right: parent.right

        Timer {
            interval: 1000
            repeat: true
            running: true
            onTriggered: {
                checkbox.checked = !checkbox.checked;
            }
        }
    }
    Slider {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 400
        value: 0.0
        minimumValue: 0
        maximumValue: 1

        PropertyAnimation on value {
            loops: Animation.Infinite
            duration: 3000
            from: 0.0
            to: 1.0
            running: true
        }
    }
}
