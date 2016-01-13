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

import QtQuick 2.1
import QtQuick.Window 2.1

Rectangle {
    id: root
    color: "black"
    property string fileName
    property alias volume: content.volume
    property bool perfMonitorsLogging: false
    property bool perfMonitorsVisible: false
    property int pixDens: Math.ceil(Screen.pixelDensity)
    property int itemWidth: 25 * pixDens
    property int itemHeight: 10 * pixDens
    property int windowWidth: Screen.desktopAvailableWidth
    property int windowHeight: Screen.desktopAvailableHeight
    property int scaledMargin: 2 * pixDens
    property int fontSize: 5 * pixDens

    QtObject {
        id: d
        property real gripSize: 20
    }

    Content {
        id: content
        color: "transparent"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: parameterPanel.top
            margins: scaledMargin
            leftMargin: scaledMargin + itemHeight
        }
        gripSize: d.gripSize
    }

    ParameterPanel {
        id: parameterPanel
        anchors {
            left: parent.left
            right: effectName.left
            bottom: parent.bottom
            margins: scaledMargin
            leftMargin: scaledMargin + itemHeight
        }
        gripSize: d.gripSize
        height: root.itemHeight * 2.5
        width: root.itemWidth * 3
    }

    Button {
        id: effectName
        anchors {
                    right: parent.right
                    bottom: perfHolder.top
                    margins: scaledMargin
                }

        text: "No effect"
        width: itemWidth * 2
        height: itemHeight
        onClicked: {
            effectName.visible = false
            listview.visible = true
            lvbg.visible = true
        }
        color: "#303030"
    }

    Rectangle {
        id: lvbg
        width: itemWidth * 2
        color: "black"
        opacity: 0.8
        visible: false

        anchors {
                    right: parent.right
                    bottom: perfHolder.top
                    top: parent.top
                    margins: scaledMargin
                }

        ListView {
            id: listview
            width: itemWidth * 2
            anchors.fill: parent
            visible: false

            model: EffectSelectionList {}
            delegate: effectDelegate

            clip: true
            focus: true

            Component {
                id: effectDelegate
                Button {
                    text: name
                    width: itemWidth * 2
                    onClicked: {
                        content.effectSource = source
                        listview.visible = false
                        lvbg.visible = false
                        effectName.text = name
                        effectName.visible = true
                        parameterPanel.model = content.effect.parameters

                    }
                }
            }
        }
    }

    Rectangle {
        id: perfHolder
        color: "transparent"
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: scaledMargin
        }
        height: root.itemHeight * 1.5
        width: root.itemWidth

        Loader {
            id: performanceLoader
            function init() {
                var enabled = root.perfMonitorsLogging || root.perfMonitorsVisible
                source = enabled ? "../performancemonitor/PerformanceItem.qml" : ""
            }
            onLoaded: {
                item.parent = perfHolder
                item.anchors.top = perfHolder.top
                item.anchors.bottom = perfHolder.bottom
                item.anchors.left = perfHolder.left
                item.anchors.right = perfHolder.right
                item.logging = root.perfMonitorsLogging
                item.displayed = root.perfMonitorsVisible
                item.init()
            }
        }
    }

    FileOpen {
        id: fileOpen
        state: "collapsed"
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            margins: scaledMargin
        }
        width: itemHeight + scaledMargin
        z: 2
        opacity: 0.9

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: fileOpen
                    width: itemWidth * 1.5
                    opacity: 0.8
                }
            },
            State {
                name: "collapsed"
                PropertyChanges {
                    target: fileOpen
                    width: itemHeight + scaledMargin
                    opacity: 0.9
                }
            }
        ]

        transitions: [
            Transition {
                NumberAnimation { target: fileOpen; property: "width"; duration: 100 }
                NumberAnimation { target: fileOpen; property: "opacity"; duration: 100 }
            }
        ]
    }

    FileBrowser {
        id: imageFileBrowser
        anchors.fill: root
        Component.onCompleted: fileSelected.connect(content.openImage)
    }

    FileBrowser {
        id: videoFileBrowser
        anchors.fill: root
        Component.onCompleted: fileSelected.connect(content.openVideo)
    }

    Component.onCompleted: {
        fileOpen.openImage.connect(openImage)
        fileOpen.openVideo.connect(openVideo)
        fileOpen.openCamera.connect(openCamera)
        fileOpen.close.connect(close)
    }

    function init() {
        if (Qt.platform.os === "linux" || Qt.platform.os === "windows" || Qt.platform.os === "osx" || Qt.platform.os === "unix") {
            if (Screen.desktopAvailableWidth > 1280) {
                windowWidth = 1280
            }
            if (Screen.desktopAvailableHeight > 720) {
                windowHeight = 720
            }
        }

        height = windowHeight
        width = windowWidth

        imageFileBrowser.folder = imagePath
        videoFileBrowser.folder = videoPath
        content.init()
        performanceLoader.init()
        if (fileName != "")
            content.openVideo(fileName)
    }

    function qmlFramePainted() {
        if (performanceLoader.item)
            performanceLoader.item.qmlFramePainted()
    }

    function openImage() {
        imageFileBrowser.show()
    }

    function openVideo() {
        videoFileBrowser.show()
    }

    function openCamera() {
        content.openCamera()
    }

    function close() {
        content.init()
    }
}
