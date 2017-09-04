/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtQuick 2.0
import QtQuick.Scene2D 2.9
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2

Rectangle {
    id: root
    color: "white"
    focus: true
    border.color: "black"
    enabled: false

    property alias colorR: color_r.value
    property alias colorG: color_g.value
    property alias colorB: color_b.value
    property alias shininess: shining.value

    property alias rotationX: rotation_x.value
    property alias rotationY: rotation_y.value
    property alias rotationZ: rotation_z.value

    property alias logoCentreZ: logoCenter_z.value


    QtObject {
        id: d
        property real rotationValue: 0
        property color textColor: root.enabled ? "black" : "gray"
    }

    SequentialAnimation {
        running: true
        paused: !animation.checked
        loops: Animation.Infinite

        NumberAnimation {
            target: d
            property: "rotationValue"
            easing.type: Easing.OutQuad
            duration: 1000
            from: 0
            to: 45
        }
        NumberAnimation {
            target: d
            property: "rotationValue"
            easing.type: Easing.InOutQuad
            duration: 1000
            from: 45
            to: -45
        }
        NumberAnimation {
            target: d
            property: "rotationValue"
            easing.type: Easing.InQuad
            duration: 1000
            from: -45
            to: 0
        }
    }

    ColumnLayout {
        id: tipLayout
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.top: root.top
        spacing: 5

        Text { text: "Middle click to (de)activate"; font.bold: true }
    }

    ColumnLayout {
        id: colorLayout
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.top: tipLayout.bottom
        anchors.topMargin: 20
        spacing: 5

        Text { text: "Appearance"; font.bold: true; color: d.textColor }
        Text { text: "Ambient color RGB"; color: d.textColor }
        RowLayout {
            Text { text: "R"; color: d.textColor }
            Slider {
                id: color_r
                Layout.fillWidth: true
                minimumValue: 0
                maximumValue: 255
                value: 128
            }
        }
        RowLayout {
            Text { text: "G"; color: d.textColor }
            Slider {
                id: color_g
                Layout.fillWidth: true
                minimumValue: 0
                maximumValue: 255
                value: 195
            }
        }
        RowLayout {
            Text { text: "B"; color: d.textColor }
            Slider {
                id: color_b
                Layout.fillWidth: true
                minimumValue: 0
                maximumValue: 255
                value: 66
            }
        }
        Text { text: "Shininess"; color: d.textColor }
        Slider {
            id: shining
            Layout.fillWidth: true
            minimumValue: 30
            maximumValue: 90
            value: 50
        }
    }

    ColumnLayout {
        id: transformLayout

        anchors.left: colorLayout.left
        anchors.right: colorLayout.right
        anchors.top: colorLayout.bottom
        anchors.topMargin: 10
        spacing: 5

        Text { text: "Item transform"; font.bold: true; color: d.textColor }
        Text { text: "Rotation"; color: d.textColor }
        RowLayout {
            Text { text: "X"; color: d.textColor }
            Slider {
                id: rotation_x
                Layout.fillWidth: true
                minimumValue: -45
                maximumValue: 45
                value: d.rotationValue
            }
        }
        RowLayout {
            Text { text: "Y"; color: d.textColor }
            Slider {
                id: rotation_y
                Layout.fillWidth: true
                minimumValue: -45
                maximumValue: 45
                value: d.rotationValue
            }
        }
        RowLayout {
            Text { text: "Z"; color: d.textColor }
            Slider {
                id: rotation_z
                Layout.fillWidth: true
                minimumValue: -45
                maximumValue: 45
                value: d.rotationValue
            }
        }

        RowLayout {
            CheckBox { id: animation; text: "Animation"; checked: false }
        }
    }

    ColumnLayout {
        id: cameraLayout

        anchors.left: colorLayout.left
        anchors.right: colorLayout.right
        anchors.top: transformLayout.bottom
        anchors.topMargin: 10
        spacing: 5

        Text { text: "Object"; font.bold: true; color: d.textColor }
        Text { text: "Logo Center Z"; color: d.textColor }
        Slider {
            id: logoCenter_z
            Layout.fillWidth: true
            minimumValue: -10
            maximumValue: 8
            value: 0
        }
    }
}
