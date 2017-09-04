/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
import QtSensors 5.0
import Grue 1.0

Rectangle {
    id: root
    width: 320
    height: 480
    color: "black"

    property int percent: 0
    property string text: ""
    property real grueOpacity: 0.0

    function updateStatus(newPercent, newOpacity, newText) {
        if (root.percent === newPercent)
            return;

        // Delay updating the visual status to prevent flicker
        timer.interval = (newPercent < root.percent) ? 500 : 0;

        root.percent = newPercent;
        root.text = newText;
        root.grueOpacity = newOpacity;

        timer.start()
    }

    Timer {
        id: timer
        running: false
        repeat: false
        onTriggered: {
            text.text = root.text
            grueimg.opacity = root.grueOpacity
        }
    }

    GrueSensor {
        id: sensor
        active: true
        onReadingChanged: {
            var percent = reading.chanceOfBeingEaten;
            if (percent === 0) {
                updateStatus(percent, 0.0, "It is light.<br>You are safe from Grues.");
            }
            else if (percent === 100) {
                updateStatus(percent, 1.0, "You have been eaten by a Grue!");
                sensor.active = false;
            }
            else if (percent > 0) {
                updateStatus(percent, 0.05 + (percent * 0.001),
                             "It is dark.<br>You are " + percent +" % " +
                             "likely to be eaten by a Grue.");
            }
        }
    }

    Text {
        id: text
        anchors.top: parent.top
        anchors.topMargin: 0
        anchors.left: parent.left
        anchors.right: parent.right
        wrapMode: Text.WordWrap
        text: "I can't tell if you're going to be eaten by a Grue or not. You're on your own!"
        font.pixelSize: 30
        color: "lightgray"
    }

    Image {
        id: grueimg
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        source: "grue.png"
        opacity: 0.0
        Behavior on opacity { PropertyAnimation { duration: 250 } }
    }
}
