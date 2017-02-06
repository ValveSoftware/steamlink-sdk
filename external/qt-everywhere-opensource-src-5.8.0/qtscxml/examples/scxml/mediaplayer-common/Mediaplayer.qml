/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

import QtQuick 2.5
import QtQuick.Window 2.2
import QtScxml 5.8

Window {
    id: root
    property StateMachine stateMachine: scxmlLoader.stateMachine
    property alias source: scxmlLoader.source

    visible: true
    width: 750
    height: 350
    color: "white"

    ListView {
        id: theList
        width: parent.width / 2
        height: parent.height
        keyNavigationWraps: true
        highlightMoveDuration: 0
        focus: true
        model: ListModel {
            id: theModel
            ListElement { media: "Song 1" }
            ListElement { media: "Song 2" }
            ListElement { media: "Song 3" }
        }
        highlight: Rectangle { color: "lightsteelblue" }
        currentIndex: -1
        delegate: Rectangle {
            height: 40
            width: parent.width
            color: "transparent"
            MouseArea {
                anchors.fill: parent;
                onClicked: tap(index)
            }
            Text {
                id: txt
                anchors.fill: parent
                text: media
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Text {
        id: theLog
        anchors.left: theList.right
        anchors.top: theText.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    Text {
        id: theText
        anchors.left: theList.right
        anchors.right: parent.right;
        anchors.top:  parent.top
        text: "Stopped"
        color: stateMachine.playing ? "green" : "red"
    }

    StateMachineLoader {
        id: scxmlLoader
    }

    EventConnection {
        stateMachine: root.stateMachine
        events: ["playbackStarted", "playbackStopped"]
        onOccurred: {
            var media = event.data.media
            if (event.name === "playbackStarted") {
                theText.text = "Playing '" + media + "'"
                theLog.text = theLog.text + "\nplaybackStarted with data: "
                                          + JSON.stringify(event.data)
            } else if (event.name === "playbackStopped") {
                theText.text = "Stopped '" + media + "'"
                theLog.text = theLog.text + "\nplaybackStopped with data: "
                                          + JSON.stringify(event.data)
            }
        }
    }

    function tap(idx) {
        var media = theModel.get(idx).media
        var data = { "media": media }
        stateMachine.submitEvent("tap", data)
    }
}
