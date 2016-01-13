/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

//! [1]
import QtQuick 2.0
import Spinner 1.0

Rectangle {

    width: 320
    height: 480

    gradient: Gradient {
        GradientStop { position: 0; color: "lightsteelblue" }
        GradientStop { position: 1; color: "black" }
    }

    Rectangle {
        color: Qt.rgba(1, 1, 1, 0.7);
        radius: 10
        border.width: 1
        border.color: "white"
        anchors.fill: blockingLabel;
        anchors.margins: -10
    }

    Text {
        id: blockingLabel
        color: blocker.running ? "red" : "black"
        text: blocker.running ? "Blocked!" : "Not blocked"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 100
    }

    Timer {
        id: blocker
        interval: 357
        running: false;
        repeat: true
        onTriggered: {
            var d = new Date();
            var x = 0;
            var wait = 50 + Math.random() * 200;
            while ((new Date().getTime() - d.getTime()) < 100) {
                x += 1;
            }
        }
    }

    Timer {
        id: blockerEnabler
        interval: 4000
        running: true
        repeat: true
        onTriggered: {
            blocker.running = !blocker.running
        }
    }

    Spinner {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: 80
        spinning: true
    }

    Image {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: -80
        source: "spinner.png"
        NumberAnimation on rotation {
            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
        }
    }

    Rectangle {
        color: Qt.rgba(1, 1, 1, 0.7)
        radius: 10
        border.width: 1
        border.color: "white"
        anchors.fill: label
        anchors.margins: -10
    }

    Text {
        id: label
        color: "black"
        wrapMode: Text.WordWrap
        text: "This application shows two spinners. The one to the right is animated on the scene graph thread (when applicable) and the left one is using the normal Qt Quick animation system."
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
    }
}
//! [2]
