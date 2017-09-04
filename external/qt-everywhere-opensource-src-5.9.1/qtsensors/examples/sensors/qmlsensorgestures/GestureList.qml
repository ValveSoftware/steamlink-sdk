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

//Import the declarative plugins
import QtQuick 2.0
import QtSensors 5.0

/* Layout
                                         gesturerect
                                        /
---------------------------------------/
|-------------------------------------|
||          labelGesture             ||
|-------------------------------------|
|-------------------------------------|
||                                   |<---- gestureListRect
||                                   ||
||                                   ||
||           gestureList             ||
||                                   ||
||                                   ||
||                                   ||
||                                   ||
||                                   ||
||                                   ||
|-------------------------------------|
*/

Rectangle {
    id: gesturerect
    border.width: 1
    anchors.margins: 5

    property string selectedGesture: ""

    SensorGesture {
        id: gesture
    }

    Text {
        id: labelGesture
        anchors.top: gesturerect.top
        anchors.left: gesturerect.left
        anchors.right: gesturerect.right
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 30
        font.bold: true
        text: "Gestures"
    }

    Rectangle {
        id: gestureListRect
        border.width: 1
        anchors.top: labelGesture.bottom
        anchors.left: gesturerect.left
        anchors.right: gesturerect.right
        anchors.bottom: gesturerect.bottom
        anchors.margins: 5

//! [4]
        ListView {
            id: gestureList
//! [4]
            anchors.fill: gestureListRect
            anchors.margins: 5
//! [5]
            model: gesture.availableGestures
//! [5]
            focus: true
            currentIndex: -1
            delegate: gestureListDelegate
            clip: true
//! [6]
        }
//! [6]

        Component {
            id: gestureListDelegate

            Rectangle {
                width: gestureList.width
                height: itemText.height
                color: (index === gestureList.currentIndex ? "#999933" : "#FFFFFF")

                Text {
                    id: itemText
                    text: model.modelData
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        gestureList.currentIndex = index
                        selectedGesture = model.modelData
                    }
                }
            }
        }
    }
}
