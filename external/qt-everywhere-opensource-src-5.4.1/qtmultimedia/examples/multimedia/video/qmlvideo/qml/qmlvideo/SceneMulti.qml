/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

Scene {
    id: root

    property real itemWidth: (width / 3) - 40
    property real itemTopMargin: 50

    QtObject {
        id: contentProxy
        function initialize() {
            video1.initialize()
            video2.initialize()
        }
    }

    Component {
        id: startStopComponent

        Rectangle {
            id: root
            color: "transparent"

            function content() {
                return root.parent
            }

            Text {
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    bottom: parent.bottom
                    margins: 20
                }
                text: content() ? content().started ? "Tap to stop" : "Tap to start" : ""
                color: "#e0e0e0"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (content().started)
                        content().stop()
                    else
                        content().start()
                }
            }
        }
    }

    Content {
        id: video1
        anchors {
            left: parent.left
            leftMargin: 10
            top: parent.top
            topMargin: root.itemTopMargin
        }
        autoStart: false
        contentType: "video"
        showBorder: true
        showFrameRate: started
        source: parent.source1
        width: itemWidth
        volume: parent.volume

        Loader {
            id: video1StartStopLoader
            onLoaded: {
                item.parent = video1
                item.anchors.fill = video1
            }
        }

        onInitialized: video1StartStopLoader.sourceComponent = startStopComponent
    }

    Rectangle {
        id: cameraHolder
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: root.itemTopMargin
        }
        border.width: 1
        border.color: "white"
        color: "transparent"
        width: itemWidth
        height: width
        property bool started: false

        Loader {
            id: cameraLoader
            onLoaded: {
                item.parent = cameraHolder
                item.centerIn = cameraHolder
                item.contentType = "camera"
                item.showFrameRate = true
                item.width = itemWidth
                item.z = 1.0
                cameraErrorConnection.target = item
                item.initialize()
            }
        }

        Loader {
            id: cameraStartStopLoader
            sourceComponent: startStopComponent
            onLoaded: {
                item.parent = cameraHolder
                item.anchors.fill = cameraHolder
                item.z = 2.0
            }
        }

        Connections {
            id: cameraErrorConnection
            onError: {
                console.log("[qmlvideo] SceneMulti.camera.onError")
                cameraHolder.stop()
            }
        }

        function start() {
            cameraLoader.source = "Content.qml"
            cameraHolder.started = true
        }

        function stop() {
            cameraLoader.source = ""
            cameraHolder.started = false
        }
    }

    Content {
        id: video2
        anchors {
            right: parent.right
            rightMargin: 10
            top: parent.top
            topMargin: root.itemTopMargin
        }
        autoStart: false
        contentType: "video"
        showBorder: true
        showFrameRate: started
        source: parent.source2
        width: itemWidth
        volume: parent.volume

        Loader {
            id: video2StartStopLoader
            onLoaded: {
                item.parent = video2
                item.anchors.fill = video2
            }
        }

        onInitialized: video2StartStopLoader.sourceComponent = startStopComponent
    }

    Component.onCompleted: root.content = contentProxy
}
