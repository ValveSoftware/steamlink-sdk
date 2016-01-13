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
import FrequencyMonitor 1.0

Rectangle {
    id: root
    property bool logging: true
    property bool displayed: true
    property bool enabled: logging || displayed
    property alias active: monitor.active
    property int samplingInterval: 500
    property color textColor: "yellow"
    property int textSize: 20
    property alias label: monitor.label

    border.width: 1
    border.color: "yellow"
    width: 5.5 * root.textSize
    height: 3.0 * root.textSize
    color: "black"
    opacity: 0.5
    radius: 10
    visible: displayed && active

    // This should ensure that the monitor is on top of all other content
    z: 999

    function notify() {
        monitor.notify()
    }

    FrequencyMonitor {
        id: monitor
        samplingInterval: root.enabled ? root.samplingInterval : 0
        onAverageFrequencyChanged: {
            if (root.logging) trace()
            averageFrequencyText.text = monitor.averageFrequency.toFixed(2)
        }
    }

    Text {
        id: labelText
        anchors {
            left: parent.left
            top: parent.top
            margins: 10
        }
        color: root.textColor
        font.pixelSize: 0.6 * root.textSize
        text: root.label
        width: root.width - 2*anchors.margins
        elide: Text.ElideRight
    }

    Text {
        id: averageFrequencyText
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 10
        }
        color: root.textColor
        font.pixelSize: root.textSize
    }
}
