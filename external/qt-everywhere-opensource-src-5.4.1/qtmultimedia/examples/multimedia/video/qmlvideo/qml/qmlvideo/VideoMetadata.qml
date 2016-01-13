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
