/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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
