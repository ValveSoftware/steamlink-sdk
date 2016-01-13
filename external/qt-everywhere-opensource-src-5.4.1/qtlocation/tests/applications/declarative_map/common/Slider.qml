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

Item {
    id: slider;
    height: 40
    // value is read/write.
    property int value
    property real minimum: 0
    property real maximum: 1
    property int xMin: 2
    property int xMax: slider.width - handle.width-xMin

    Rectangle {
        anchors.fill: parent
        border.width: 0;
        radius: 8
        color: "dimgrey"
        opacity: 0.6
    }

    Rectangle {
        id: handle; smooth: true
        width: 30;
        y: xMin;
        x: xMin + (value - minimum) * slider.xMax / (maximum - minimum)

        height: slider.height-4; radius: 6
        gradient: normalGradient

        Gradient {
            id: normalGradient
            GradientStop { position: 0.0; color: "lightgrey" }
            GradientStop { position: 1.0; color: "gray" }
        }

        Gradient {
            id: pressedGradient
            GradientStop { position: 0.0; color: "lightgray" }
            GradientStop { position: 1.0; color: "black" }
        }

        Gradient {
            id: hoveredGradient
            GradientStop { position: 0.0; color: "lightgrey" }
            GradientStop { position: 1.0; color: "dimgrey" }
        }

        MouseArea {
            id: mouseRegion
            hoverEnabled: true
            anchors.fill: parent; drag.target: parent
            drag.axis: Drag.XAxis; drag.minimumX: slider.xMin; drag.maximumX: slider.xMax
            onPositionChanged: { value = (maximum - minimum) * (handle.x-slider.xMin) / (slider.xMax - slider.xMin) + minimum; }
        }
    }

    states: [
        State {
            name: "Pressed"
            when: mouseRegion.pressed
            PropertyChanges { target: handle; gradient: pressedGradient }
        },
        State {
            name: "Hovered"
            when: mouseRegion.containsMouse
            PropertyChanges { target: handle; gradient: hoveredGradient }
        }
    ]
}
