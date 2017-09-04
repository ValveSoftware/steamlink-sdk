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
