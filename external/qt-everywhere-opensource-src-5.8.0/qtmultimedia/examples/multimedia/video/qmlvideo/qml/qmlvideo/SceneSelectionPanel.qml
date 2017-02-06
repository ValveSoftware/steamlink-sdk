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
    property int itemHeight: 25
    property string sceneSource: ""

    ListModel {
        id: videolist
        ListElement { name: "Multi"; source: "SceneMulti.qml" }
        ListElement { name: "Video"; source: "VideoBasic.qml" }
        ListElement { name: "Drag"; source: "VideoDrag.qml" }
        ListElement { name: "Fillmode"; source: "VideoFillMode.qml" }
        ListElement { name: "Fullscreen"; source: "VideoFullScreen.qml" }
        ListElement { name: "Fullscreen-inverted"; source: "VideoFullScreenInverted.qml" }
        ListElement { name: "Metadata"; source: "VideoMetadata.qml" }
        ListElement { name: "Move"; source: "VideoMove.qml" }
        ListElement { name: "Overlay"; source: "VideoOverlay.qml" }
        ListElement { name: "Playback Rate"; source: "VideoPlaybackRate.qml" }
        ListElement { name: "Resize"; source: "VideoResize.qml" }
        ListElement { name: "Rotate"; source: "VideoRotate.qml" }
        ListElement { name: "Spin"; source: "VideoSpin.qml" }
        ListElement { name: "Seek"; source: "VideoSeek.qml" }
    }

    ListModel {
        id: cameralist
        ListElement { name: "Camera"; source: "CameraBasic.qml" }
        ListElement { name: "Drag"; source: "CameraDrag.qml" }
        ListElement { name: "Fullscreen"; source: "CameraFullScreen.qml" }
        ListElement { name: "Fullscreen-inverted"; source: "CameraFullScreenInverted.qml" }
        ListElement { name: "Move"; source: "CameraMove.qml" }
        ListElement { name: "Overlay"; source: "CameraOverlay.qml" }
        ListElement { name: "Resize"; source: "CameraResize.qml" }
        ListElement { name: "Rotate"; source: "CameraRotate.qml" }
        ListElement { name: "Spin"; source: "CameraSpin.qml" }
    }

    Component {
        id: leftDelegate
        Item {
            width: root.width / 2
            height: 0.8 * itemHeight

            Button {
                anchors.fill: parent
                anchors.margins: 5
                anchors.rightMargin: 2.5
                anchors.bottomMargin: 0
                text: name
                onClicked: root.sceneSource = source
            }
        }
    }

    Component {
        id: rightDelegate
        Item {
            width: root.width / 2
            height: 0.8 * itemHeight

            Button {
                anchors.fill: parent
                anchors.margins: 5
                anchors.leftMargin: 2.5
                anchors.bottomMargin: 0
                text: name
                onClicked: root.sceneSource = source
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: (itemHeight * videolist.count) + 10
        clip: true

        Row {
            id: layout
            anchors {
                fill: parent
                topMargin: 5
                bottomMargin: 5
            }

            Column {
                Repeater {
                    model: videolist
                    delegate: leftDelegate
                }
            }

            Column {
                Repeater {
                    model: cameralist
                    delegate: rightDelegate
                }
            }
        }
    }
}
