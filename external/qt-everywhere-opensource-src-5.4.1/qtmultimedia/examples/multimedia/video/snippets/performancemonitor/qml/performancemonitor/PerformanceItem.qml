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
