/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
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

import QtQuick 2.0
import "draw.js" as DrawGraph

Rectangle {
    id: results
    color: "#F0EBED"

    Component.onCompleted:  heartRate.obtainResults()

    function getTime() {
        var t = heartRate.time;
        var min = Math.floor(t/60);
        var sec = t%60;
        var r = min + " min " + sec + " sec ";
        return r;
    }

    function drawGraph() {
        var b = plot.height/200;
        var ctx = canvas1.getContext('2d');
        ctx.beginPath()
        ctx.moveTo(10, plot.height- (b*60))
        var size = heartRate.measurementsSize();
        var difference = (plot.width-topbar.width)/size;

        for (var i = 0; i< size; i++) {
            var value = heartRate.measurements(i);
            if (i == 0) {
                ctx.moveTo(10+2, (plot.height- (value*b) + 2));
                ctx.rect((10 + i*difference), (plot.height- (value*b)), 4, 4);

            }
            else {
                ctx.lineTo((10+2 + i*difference), (plot.height- (value*b) + 2));
                ctx.rect((10 + i*difference), (plot.height- (value*b)), 4, 4);
            }

        }
        ctx.fillStyle = "#3870BA"
        ctx.fill()
        ctx.strokeStyle = "#3870BA"
        ctx.stroke()
        ctx.closePath()
    }

    Rectangle {
        id: res
        width: parent.width
        anchors.top: parent.top
        height: 80
        color: "#F0EBED"
        border.color: "#3870BA"
        border.width: 2
        radius: 10
        Text {
            id: restText
            color: "#3870BA"
            font.pixelSize: 34
            anchors.centerIn: parent
            text: "Results"
        }
    }

    Text {
        id: topbar
        text: "200"
        anchors.left: parent.left
        anchors.top: res.bottom
        anchors.rightMargin: 4
        color: "#3870BA"
        z: 50
    }

    Rectangle {
        id: level
        anchors.left: topbar.right

        anchors.top: res.bottom
        height: ((results.height -(res.height + menuLast.height + start.height ))/2)
        width: 3
        color: "#3870BA"
    }

    Text {
        id: middlebar
        anchors.verticalCenter: level.verticalCenter
        anchors.left: parent.left
        text: "100"
        color: "#3870BA"
        z: 50
    }

    Rectangle{
        id: downlevel
        anchors.bottom: cover.top
        width: parent.width
        anchors.left: level.right
        height: 3
        color: "#3870BA"
        z: 50
    }

    Rectangle {
        id: plot
        anchors.left: level.right
        anchors.leftMargin: 15
        width: results.width
        height: ((parent.height-(res.height+menuLast.height+start.height))/2)

        anchors.top: res.bottom
        color: "#F0EBED"
        Canvas {
            id: canvas1
            anchors.fill: parent
            z: 150
            onPaint: drawGraph()
        }
    }

    Rectangle {
        id: cover
        anchors.top: plot.bottom
        anchors.bottom: menuLast.top
        width: parent.width
        height: ((parent.height-(res.height+menuLast.height+start.height))/2)
        color: "#F0EBED"
        radius: 10
        border.color: "#3870BA"
        border.width: 2

        Flickable {
            id: scroll
            anchors.fill: parent
            anchors.margins: 5
            clip: true
            contentWidth: parent.width
            contentHeight: stresult.height

            Rectangle {
                id: stresult
                width: parent.width
                height: (results.height - (res.height + menuLast.height + start.height - 100))
                color: "#F0EBED"
                radius: 10

                Text {
                    id: averageHR
                    font.pixelSize: 30;
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top

                    color: "#3870BA"
                    text: "Average Heart Rate"
                }

                Text {
                    id: averageHRt
                    font.pixelSize: 40; font.bold: true
                    anchors.top: averageHR.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: heartRate.average
                }

                Text {
                    id: time
                    font.pixelSize: 30;
                    anchors.top: averageHRt.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: "Seconds measured:"
                }

                Text {
                    id: timet
                    font.pixelSize:  40; font.bold: true
                    anchors.top: time.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: getTime()
                }
                Text {
                    id: maxi
                    font.pixelSize:  30;
                    anchors.top: timet.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 20
                    color: "#3870BA"
                    text: "  Max  ||  Min "
                }

                Text {
                    id: mini
                    font.pixelSize:  40; font.bold: true
                    anchors.top:maxi.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: " " + heartRate.maxHR + " || " + heartRate.minHR
                }

                Text {
                    id: calories
                    font.pixelSize:  30;
                    anchors.top: mini.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: "  Calories "
                }

                Text {
                    id: caloriestext
                    font.pixelSize:  40; font.bold: true
                    anchors.top:calories.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 10
                    color: "#3870BA"
                    text: heartRate.calories.toFixed(3)
                }
            }
        }
    }

    Button {
        id:menuLast
        buttonWidth: parent.width
        buttonHeight: 0.1*parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: start.top
        text: "Menu"
        onButtonClick: { pageLoader.source="main.qml"}
    }

    Button {
        id:start
        buttonWidth: parent.width
        buttonHeight: 0.1*parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        text: "Start Monitoring"
        onButtonClick: {
            heartRate.connectToService(heartRate.deviceAddress());
            pageLoader.source="monitor.qml";
        }
    }
}
