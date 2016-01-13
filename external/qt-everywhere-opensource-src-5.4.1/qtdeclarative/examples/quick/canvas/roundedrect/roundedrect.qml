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
import "../contents"
import "../../shared"

Item {
    id:container
    width: 320
    height: 480

    Column {
        spacing: 6
        anchors.fill: parent
        anchors.topMargin: 12
        Text {
            font.pointSize: 24
            font.bold: true
            text: "Rounded rectangle"
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#777"
        }
        Canvas {
            id: canvas
            width: 320
            height: 280
            antialiasing: true

            property int radius: rCtrl.value
            property int rectx: 60
            property int recty: 60
            property int rectWidth: width - 2*rectx
            property int rectHeight: height - 2*recty
            property color strokeStyle:  Qt.darker(fillStyle, 1.4)
            property color fillStyle: "#ae32a0" // purple
            property int lineWidth: lineWidthCtrl.value
            property bool fill: true
            property bool stroke: true
            property real alpha: 1.0

            onLineWidthChanged:requestPaint();
            onFillChanged:requestPaint();
            onStrokeChanged:requestPaint();
            onRadiusChanged:requestPaint();

            onPaint: {
                var ctx = getContext("2d");
                ctx.save();
                ctx.clearRect(0,0,canvas.width, canvas.height);
                ctx.strokeStyle = canvas.strokeStyle;
                ctx.lineWidth = canvas.lineWidth
                ctx.fillStyle = canvas.fillStyle
                ctx.globalAlpha = canvas.alpha
                ctx.beginPath();
                ctx.moveTo(rectx+radius,recty);                 // top side
                ctx.lineTo(rectx+rectWidth-radius,recty);
                // draw top right corner
                ctx.arcTo(rectx+rectWidth,recty,rectx+rectWidth,recty+radius,radius);
                ctx.lineTo(rectx+rectWidth,recty+rectHeight-radius);    // right side
                // draw bottom right corner
                ctx.arcTo(rectx+rectWidth,recty+rectHeight,rectx+rectWidth-radius,recty+rectHeight,radius);
                ctx.lineTo(rectx+radius,recty+rectHeight);              // bottom side
                // draw bottom left corner
                ctx.arcTo(rectx,recty+rectHeight,rectx,recty+rectHeight-radius,radius);
                ctx.lineTo(rectx,recty+radius);                 // left side
                // draw top left corner
                ctx.arcTo(rectx,recty,rectx+radius,recty,radius);
                ctx.closePath();
                if (canvas.fill)
                    ctx.fill();
                if (canvas.stroke)
                    ctx.stroke();
                ctx.restore();
            }
        }
    }
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        Slider {id: lineWidthCtrl ; min: 1 ; max: 10; init: 2 ; name: "Outline"}
        Slider {id: rCtrl ; min: 10 ; max: 80 ; init: 40 ; name: "Radius"}
    }
}
