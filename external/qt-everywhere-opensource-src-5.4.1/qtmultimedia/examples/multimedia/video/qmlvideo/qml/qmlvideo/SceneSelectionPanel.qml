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
