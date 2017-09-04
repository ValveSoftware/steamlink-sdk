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

Rectangle {
    id: root
    anchors.fill: parent
    color: "black"

    property string source1
    property string source2
    property color bgColor: "black"
    property real volume: 0.25
    property bool perfMonitorsLogging: false
    property bool perfMonitorsVisible: false

    QtObject {
        id: d
        property int itemHeight: root.height > root.width ? root.width / 10 : root.height / 10
        property int buttonHeight: 0.8 * itemHeight
        property int margins: 5
    }

    Loader {
        id: performanceLoader

        Connections {
            target: inner
            onVisibleChanged:
                if (performanceLoader.item)
                    performanceLoader.item.enabled = !inner.visible
            ignoreUnknownSignals: true
        }

        function init() {
            var enabled = root.perfMonitorsLogging || root.perfMonitorsVisible
            source = enabled ? "../performancemonitor/PerformanceItem.qml" : ""
        }

        onLoaded: {
            item.parent = root
            item.anchors.fill = root
            item.logging = root.perfMonitorsLogging
            item.displayed = root.perfMonitorsVisible
            item.enabled = false
            item.init()
        }
    }

    Rectangle {
        id: inner
        anchors.fill: parent
        color: root.bgColor

        Button {
            id: openFile1Button
            anchors {
                top: parent.top
                left: parent.left
                right: exitButton.left
                margins: d.margins
            }
            bgColor: "#212121"
            bgColorSelected: "#757575"
            textColorSelected: "white"
            height: d.buttonHeight
            text: (root.source1 == "") ? "Select file 1" : root.source1
            onClicked: fileBrowser1.show()
        }

        Button {
            id: openFile2Button
            anchors {
                top: openFile1Button.bottom
                left: parent.left
                right: exitButton.left
                margins: d.margins
            }
            bgColor: "#212121"
            bgColorSelected: "#757575"
            textColorSelected: "white"
            height: d.buttonHeight
            text: (root.source2 == "") ? "Select file 2" : root.source2
            onClicked: fileBrowser2.show()
        }

        Button {
            id: exitButton
            anchors {
                top: parent.top
                right: parent.right
                margins: d.margins
            }
            bgColor: "#212121"
            bgColorSelected: "#757575"
            textColorSelected: "white"
            width: parent.width / 10
            height: d.buttonHeight
            text: "Exit"
            onClicked: Qt.quit()
        }

        Row {
            id: modes
            anchors.top: openFile2Button.bottom
            anchors.margins: 0
            anchors.topMargin: 5
            Button {
                width: root.width / 2
                height: 0.8 * d.itemHeight
                bgColor: "#212121"
                radius: 0
                text: "Video Modes"
                enabled: false
            }
            Button {
                width: root.width / 2
                height: 0.8 * d.itemHeight
                bgColor: "#212121"
                radius: 0
                text: "Camera Modes"
                enabled: false
            }
        }

        Rectangle {
            id: divider
            height: 1
            width: parent.width
            color: "black"
            anchors.top: modes.bottom
        }

        SceneSelectionPanel {
            id: sceneSelectionPanel
            itemHeight: d.itemHeight
            color: "#212121"
            anchors {
                top: divider.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            radius: 0
            onSceneSourceChanged: {
                sceneLoader.source = sceneSource
                var scene = null
                var innerVisible = true
                if (sceneSource == "") {
                    if (performanceLoader.item)
                        performanceLoader.item.videoActive = false
                } else {
                    scene = sceneLoader.item
                    if (scene) {
                        if (scene.contentType === "video" && source1 === "") {
                            errorDialog.show("You must first select a video file")
                            sceneSource = ""
                        } else {
                            scene.parent = root
                            scene.color = root.bgColor
                            scene.buttonHeight = d.buttonHeight
                            scene.source1 = source1
                            scene.source2 = source2
                            scene.volume = volume
                            scene.anchors.fill = root
                            scene.close.connect(closeScene)
                            scene.content.initialize()
                            innerVisible = false
                        }
                    }
                }
                videoFramePaintedConnection.target = scene
                inner.visible = innerVisible
            }
        }
    }

    Loader {
        id: sceneLoader
    }

    Connections {
        id: videoFramePaintedConnection
        onVideoFramePainted: {
            if (performanceLoader.item)
                performanceLoader.item.videoFramePainted()
        }
        ignoreUnknownSignals: true
    }

    FileBrowser {
        id: fileBrowser1
        anchors.fill: root
        onFolderChanged: fileBrowser2.folder = folder
        Component.onCompleted: fileSelected.connect(root.openFile1)
    }

    FileBrowser {
        id: fileBrowser2
        anchors.fill: root
        onFolderChanged: fileBrowser1.folder = folder
        Component.onCompleted: fileSelected.connect(root.openFile2)
    }

    function openFile1(path) {
        root.source1 = path
    }

    function openFile2(path) {
        root.source2 = path
    }

    ErrorDialog {
        id: errorDialog
        anchors.fill: root
        dialogWidth: d.itemHeight * 5
        dialogHeight: d.itemHeight * 3
        enabled: false
    }

    // Called from main() once root properties have been set
    function init() {
        performanceLoader.init()
        fileBrowser1.folder = videoPath
        fileBrowser2.folder = videoPath
    }

    function qmlFramePainted() {
        if (performanceLoader.item)
            performanceLoader.item.qmlFramePainted()
    }

    function closeScene() {
        sceneSelectionPanel.sceneSource = ""
    }
}
