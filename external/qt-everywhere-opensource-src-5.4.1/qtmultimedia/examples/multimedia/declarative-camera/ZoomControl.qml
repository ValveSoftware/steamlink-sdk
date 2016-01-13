/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
import QtMultimedia 5.0

Item {
    id : zoomControl
    property real currentZoom : 1
    property real maximumZoom : 1
    signal zoomTo(real value)

    visible: zoomControl.maximumZoom > 1

    MouseArea {
        id : mouseArea
        anchors.fill: parent

        property real initialZoom : 0
        property real initialPos : 0

        onPressed: {
            initialPos = mouseY
            initialZoom = zoomControl.currentZoom
        }

        onPositionChanged: {
            if (pressed) {
                var target = initialZoom * Math.pow(5, (initialPos-mouseY)/zoomControl.height);
                target = Math.max(1, Math.min(target, zoomControl.maximumZoom))
                zoomControl.zoomTo(target)
            }
        }
    }

    Item {
        id : bar
        x : 16
        y : parent.height/4
        width : 24
        height : parent.height/2

        Rectangle {
            anchors.fill: parent

            smooth: true
            radius: 8
            border.color: "white"
            border.width: 2
            color: "black"
            opacity: 0.3
        }

        Rectangle {
            id: groove
            x : 0
            y : parent.height * (1.0 - (zoomControl.currentZoom-1.0) / (zoomControl.maximumZoom-1.0))
            width: parent.width
            height: parent.height - y
            smooth: true
            radius: 8
            color: "white"
            opacity: 0.5
        }

        Text {
            id: zoomText
            anchors {
                left: bar.right; leftMargin: 16
            }
            y: Math.min(parent.height - height, Math.max(0, groove.y - height / 2))
            text: "x" + Math.round(zoomControl.currentZoom * 100) / 100
            font.bold: true
            color: "white"
            style: Text.Raised; styleColor: "black"
            opacity: 0.85
            font.pixelSize: 18
        }
    }
}
