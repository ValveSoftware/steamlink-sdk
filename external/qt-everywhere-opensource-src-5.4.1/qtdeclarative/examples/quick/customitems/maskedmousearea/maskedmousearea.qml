/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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
import Example 1.0

Rectangle {
    height: 480
    width: 320
    color: "black"

    Text {
        text: qsTr("CLICK AND HOVER")
        opacity: 0.6
        color: "white"
        font.pixelSize: 20
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 50
    }

    Image {
        id: moon
        anchors.centerIn: parent
        scale: moonArea.pressed ? 1.1 : 1.0
        opacity: moonArea.containsMouse ? 1.0 : 0.7
        source: Qt.resolvedUrl("images/moon.png")

        MaskedMouseArea {
            id: moonArea
            anchors.fill: parent
            alphaThreshold: 0.4
            maskSource: moon.source
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
    }

    Image {
        id: rightCloud
        anchors {
            centerIn: moon
            verticalCenterOffset: 30
            horizontalCenterOffset: 80
        }
        scale: rightCloudArea.pressed ? 1.1 : 1.0
        opacity: rightCloudArea.containsMouse ? 1.0 : 0.7
        source: Qt.resolvedUrl("images/cloud_2.png")

        MaskedMouseArea {
            id: rightCloudArea
            anchors.fill: parent
            alphaThreshold: 0.4
            maskSource: rightCloud.source
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
    }

    Image {
        id: leftCloud
        anchors {
            centerIn: moon
            verticalCenterOffset: 40
            horizontalCenterOffset: -80
        }
        scale: leftCloudArea.pressed ? 1.1 : 1.0
        opacity: leftCloudArea.containsMouse ? 1.0 : 0.7
        source: Qt.resolvedUrl("images/cloud_1.png")

        MaskedMouseArea {
            id: leftCloudArea
            anchors.fill: parent
            alphaThreshold: 0.4
            maskSource: leftCloud.source
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
    }
}
