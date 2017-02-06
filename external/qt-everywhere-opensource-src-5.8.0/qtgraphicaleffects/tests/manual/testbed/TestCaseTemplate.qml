/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

Item {
    id: main

    property real imageSize: Math.min(height, width - 220)
    default property alias testItem : testItemContainer.children
    property alias controls: controlsColumn.children
    property string currentTest: ""
    property string fps: "nan"
    property color bgColor: "black"

    property int dummy: 0
    property int fpsCount: 0

    anchors.fill: parent

    onDummyChanged: fpsCount++;

    NumberAnimation on dummy {
        duration: 500
        from: 0
        to: 10000
        loops: Animation.Infinite
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            fps = "" + fpsCount;
            fpsCount = 0;
        }
    }

    Rectangle {
        id: backgroundColor
        anchors.fill: testItemContainer
        color: bgColor
    }

    Image {
        id: background
        anchors.fill: testItemContainer
        fillMode: Image.Tile
        source: bgColor.toString() == "#010101" ? "images/background.png" : ""
    }

    Item {
        id: testItemContainer
        property real margin: 0
        x: (parent.width - testParameterContainer.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        anchors.top: undefined
        width: Math.min(parent.height - 20, parent.width - testParameterContainer.width - 20)
        height: width
    }

    Image {
        id: titlebar
        source: "images/title.png"
        anchors.top: parent.top
        anchors.right: parent.right
        width: 300

        Text {
            id: effectsListTitle
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: currentTest
            color: "white"
            font.family: "Arial"
            font.bold: true
            font.pixelSize: 12
        }
    }

    Rectangle {
        id: testParameterContainer
        anchors.top: titlebar.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 300
        color: "#171717"

        Flickable {
            id: slidersFlickable
            anchors.fill: parent
            contentHeight: controlsColumn.height
            interactive: contentHeight > height
            clip: true

            Column {
                id: controlsColumn
                anchors.left: parent.left
                anchors.right: parent.right
            }
        }
    }
}
