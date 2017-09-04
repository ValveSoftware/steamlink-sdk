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
    property string contentType: "video"

    Content {
        id: content
        anchors.centerIn: parent
        width: parent.contentWidth
        contentType: "video"
        source: parent.source1
        volume: parent.volume
        onInitialized: {
            if (!dummy)
                metadata.createObject(root)
        }
        onVideoFramePainted: root.videoFramePainted()
    }

    Component {
        id: metadata
        Column {
            anchors.fill: parent
            Text {
                color: "#e0e0e0"
                text: "Title:" + content.contentItem().metaData.title
            }
            Text {
                color: "#e0e0e0"
                text: "Size:" + content.contentItem().metaData.size
            }
            Text {
                color: "#e0e0e0"
                text: "Resolution:" + content.contentItem().metaData.resolution
            }
            Text {
                color: "#e0e0e0"
                text: "Media type:" + content.contentItem().metaData.mediaType
            }
            Text {
                color: "#e0e0e0"
                text: "Video codec:" + content.contentItem().metaData.videoCodec
            }
            Text {
                color: "#e0e0e0"
                text: "Video bit rate:" + content.contentItem().metaData.videoBitRate
            }
            Text {
                color: "#e0e0e0"
                text: "Video frame rate:" +content.contentItem().metaData.videoFrameRate
            }
            Text {
                color: "#e0e0e0"
                text: "Audio codec:" + content.contentItem().metaData.audioCodec
            }
            Text {
                color: "#e0e0e0"
                text: "Audio bit rate:" + content.contentItem().metaData.audioBitRate
            }
            Text {
                color: "#e0e0e0"
                text: "Date:" + content.contentItem().metaData.date
            }
            Text {
                color: "#e0e0e0"
                text: "Description:" + content.contentItem().metaData.description
            }
            Text {
                color: "#e0e0e0"
                text: "Copyright:" + content.contentItem().metaData.copyright
            }
            Text {
                color: "#e0e0e0"
                text: "Seekable:" + content.contentItem().metaData.seekable
            }
        }
    }

    Component.onCompleted: root.content = content
}
