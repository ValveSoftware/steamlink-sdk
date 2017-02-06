/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

//Import the declarative plugins
import QtQuick 2.0

//! [0]
import QtSensors 5.0
//! [0]

/* Layout
                                         gesturerect
                                        /
---------------------------------------/
| ----------------------------------- |
| |           id: titleText         | |
| ----------------------------------- |
|                                     |
|                                     |
| ----------------------------------- |
| |           id: detectionText     | |
| ----------------------------------- |
|                                     |
|                                     |
|                                     |
| ----------------------------------- |
| |           id: valueText         | |
| ----------------------------------- |
|                                     |
|                                     |
|                                     |
| ------------------------------      |
| | id: gestureStartStopButton |      |
| ------------------------------      |
---------------------------------------
*/

Rectangle {
    id: gestureRect
    border.width: 1
    anchors.margins: 5
//! [2]
    property alias gestureId: sensorGesture.gestures
//! [2]
    property alias gestureTitle: titleText.text
    property alias enabled: sensorGesture.enabled
    property string oldGesture: ""
    property int count: 0

//! [1]
    SensorGesture {
        id: sensorGesture
        enabled: false
        onDetected: {
            if (gesture !== oldGesture)
                count = 0;
            valueText.text = gesture + " " + count;
            oldGesture = gesture;
            count++;
        }
        onEnabledChanged: {
            valueText.text = ""
        }
    }
//! [1]

    Text {
        id: titleText
        anchors.top: gestureRect.top
        anchors.left: gestureRect.left
        anchors.right: gestureRect.right
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 20
        font.bold: true
        text: ""
    }

    Text {
        id: detectionText
        anchors.top: titleText.bottom
        anchors.left: gestureRect.left
        anchors.right: gestureRect.right
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 15
        text: "Detection:"
    }

    Text {
        id: valueText
        anchors.top: detectionText.bottom
        anchors.left: gestureRect.left
        anchors.right: gestureRect.right
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 15
        visible: sensorGesture.enabled
    }

    Button{
        id: gestureStartStopButton
        anchors.left: gestureRect.left
        anchors.bottom: gestureRect.bottom
        height: 30
        width: 100
        buttonText: (sensorGesture.enabled ? "Stop" : "Start")
        enabled: true;
        onClicked: {
            if (gestureStartStopButton.buttonText === "Start") {
                sensorGesture.enabled = true;
            }
            else {
                sensorGesture.enabled = false;
            }
        }
    }
}
