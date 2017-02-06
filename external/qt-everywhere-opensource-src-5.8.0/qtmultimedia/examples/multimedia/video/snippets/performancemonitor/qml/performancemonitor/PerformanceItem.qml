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
    property bool logging: true
    property bool displayed: true
    property bool videoActive
    property int margins: 5
    property bool enabled: true

    color: "transparent"

    // This should ensure that the monitor is on top of all other content
    z: 999

    Column {
        id: column
        anchors {
            fill: root
            margins: 10
        }
        spacing: 10
    }

    QtObject {
        id: d
        property Item qmlFrameRateItem: null
        property Item videoFrameRateItem: null
    }

    Connections {
        id: videoFrameRateActiveConnections
        ignoreUnknownSignals: true
        onActiveChanged: root.videoActive = videoFrameRateActiveConnections.target.active
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges {
                target: root
                opacity: 0
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "*"
            NumberAnimation {
                properties: "opacity"
                easing.type: Easing.OutQuart
                duration: 500
            }
        }
    ]

    state: enabled ? "baseState" : "hidden"

    function createQmlFrameRateItem() {
        var component = Qt.createComponent("../frequencymonitor/FrequencyItem.qml")
        if (component.status == Component.Ready)
            d.qmlFrameRateItem = component.createObject(column, { label: "QML frame rate",
                                                                   displayed: root.displayed,
                                                                  logging: root.logging
                                                                })
    }

    function createVideoFrameRateItem() {
        var component = Qt.createComponent("../frequencymonitor/FrequencyItem.qml")
        if (component.status == Component.Ready)
            d.videoFrameRateItem = component.createObject(column, { label: "Video frame rate",
                                                                     displayed: root.displayed,
                                                                    logging: root.logging
                                                                  })
        videoFrameRateActiveConnections.target = d.videoFrameRateItem
    }


    function init() {
        createQmlFrameRateItem()
        createVideoFrameRateItem()
    }

    function videoFramePainted() {
        if (d.videoFrameRateItem)
            d.videoFrameRateItem.notify()
    }

    function qmlFramePainted() {
        if (d.qmlFrameRateItem)
            d.qmlFrameRateItem.notify()
    }

    onVideoActiveChanged: {
        if (d.videoFrameRateItem)
            d.videoFrameRateItem.active = root.videoActive
    }
}
