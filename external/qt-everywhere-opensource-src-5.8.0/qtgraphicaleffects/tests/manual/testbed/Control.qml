/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

Item {
    id: control
    property string caption: ""
    default property alias children: controlsColumn.children
    property bool last: false

    property bool __hide: caption == "advanced"

    anchors {left: parent.left; right: parent.right}
    height: __hide ? 30 : controlsColumn.height + 40

    Behavior on height {
        id: heightBehavior
        enabled: false
        NumberAnimation { duration: 100 }
    }

    Image {
        source: "images/group_top.png"
        anchors {top: parent.top; left: parent.left; right: parent.right}
    }

    Image {
        source: "images/group_bottom.png"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        smooth: true
        visible: !last || __hide
    }

    Text {
        id: sectionText
        anchors {left: parent.left; top: parent.top; leftMargin: 11; topMargin: 8}
        color: "white"
        font.family: "Arial"
        font.bold: true
        font.pixelSize: 12
        text: caption
    }

    Image {
        anchors {right: parent.right; rightMargin: 5; top: parent.top}
        source: __hide ? "images/expand.png" : "images/collapse.png"
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        onClicked: {
            heightBehavior.enabled = true
            control.__hide = !control.__hide
        }
    }

    Column {
        id: controlsColumn
        anchors.top: parent.top
        anchors.topMargin: 30
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: !__hide
        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }
    }
}
