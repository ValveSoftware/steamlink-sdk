/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the manual tests of the Qt Toolkit.
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

import QtQuick 2.4
import "qrc:/quick/shared/" as Examples

Rectangle {
    id: root

    Item {
        id: flickArea
        anchors.fill: parent
        anchors.bottomMargin: bottomFlow.implicitHeight + 4
        clip: true

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: text.implicitWidth
            contentHeight: text.implicitHeight
            pixelAligned: pxAlignCB.checked
            Text {
                id: text
                text: "foo bar"
                font.family: "mono"
            }
            onContentXChanged: canvas.requestPaint()
            onContentYChanged: canvas.requestPaint()
        }

        Timer { id: fadeTimer; interval: 1000; onTriggered: { hfade.start(); } }

        MouseArea {
            id: verticalScrollArea
            anchors {
                right: parent.right
                top: flick.top
                bottom: flick.bottom
            }
            width: 36
            onMouseYChanged: {
                var contentY = Math.min(height, mouseY) / height * (flick.contentHeight - height)
                flick.contentY = Math.max(0, contentY)
            }
            onReleased: flick.returnToBounds()
            Rectangle {
                anchors.right: parent.right
                anchors.margins: 2
                color: "darkgrey"
                width: 20
                radius: 2
                antialiasing: true
                height: flick.height * (flick.height / flick.contentHeight) - anchors.margins * 2
                y: flick.contentY * (flick.height / flick.contentHeight)

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 6
                    radius: 2
                    color: "blue"
                    visible: flick.atYBeginning
                }

                Rectangle {
                    anchors.top: parent.bottom
                    width: parent.width
                    height: 6
                    radius: 2
                    color: "blue"
                    visible: flick.atYEnd
                }

                Text {
                    anchors.centerIn: parent
                    text: flick.contentY.toFixed(2)
                    rotation: 90
                    style: Text.Outline
                    styleColor: "white"
                    color: "black"
                }

            }
        }

        Rectangle {
            id: horizontalScrollDecorator
            anchors.bottom: flick.bottom
            anchors.bottomMargin: -4
            color: "darkgrey"
            border.color: "black"
            border.width: 1
            height: 5
            radius: 2
            antialiasing: true
            width: flick.width * (flick.width / flick.contentWidth) - (height - anchors.margins) * 2
            x:  flick.contentX * (flick.width / flick.contentWidth)
            NumberAnimation on opacity { id: hfade; to: 0; duration: 500 }
            onXChanged: { opacity = 1.0; fadeTimer.restart() }
        }

        Canvas {
            id: canvas
            anchors.fill: parent
            antialiasing: true
            renderTarget: Canvas.FramebufferObject
            onPaint: {
                var ctx = canvas.getContext('2d');
                ctx.save()
                ctx.clearRect(0, 0, canvas.width, canvas.height)
                ctx.strokeStyle = "green"
                ctx.fillStyle = "green"
                ctx.lineWidth = 1

                if (flick.horizontalVelocity) {
                    ctx.save()
                    ctx.beginPath()
                    ctx.translate((flick.horizontalVelocity < 0 ? width : 0), height / 2)
                    ctx.moveTo(0, 0)
                    var velScaled = flick.horizontalVelocity / 10
                    var arrowOffset = (flick.horizontalVelocity < 0 ? 10 : -10)
                    ctx.lineTo(velScaled, 0)
                    ctx.lineTo(velScaled + arrowOffset, -4)
                    ctx.lineTo(velScaled + arrowOffset, 4)
                    ctx.lineTo(velScaled, 0)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.fill()
                    ctx.restore()
                }

                if (flick.verticalVelocity) {
                    ctx.save()
                    ctx.beginPath()
                    ctx.translate(width / 2, (flick.verticalVelocity < 0 ? height : 0))
                    ctx.moveTo(0, 0)
                    var velScaled = flick.verticalVelocity / 10
                    var arrowOffset = (flick.verticalVelocity < 0 ? 10 : -10)
                    ctx.lineTo(0, velScaled)
                    ctx.lineTo(-4, velScaled + arrowOffset)
                    ctx.lineTo(4, velScaled + arrowOffset)
                    ctx.lineTo(0, velScaled)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.fill()
                    ctx.restore()
                }

                ctx.restore()
            }
        }
    }

    Row {
        id: bottomFlow
        anchors.bottom: parent.bottom
        width: parent.width - 12
        x: 6
        spacing: 12

        Item {
            id: progFlickItem
            width: progFlickRow.implicitWidth
            height: progFlickRow.implicitHeight + 4 + flickingLabel.implicitHeight
            Text { id: progLabel; text: "programmatic flick: h " + xvelSlider.value.toFixed(1) + " v " + yvelSlider.value.toFixed(1) }
            Row {
                id: progFlickRow
                y: progLabel.height
                spacing: 4

                Column {
                    Examples.Slider {
                        id: xvelSlider
                        min: -5000
                        max: 5000
                        init: 5000
                        width: 250
                        name: "X"
                        minLabelWidth: 0
                    }
                    Examples.Slider {
                        id: yvelSlider
                        min: -5000
                        max: 5000
                        init: 2500
                        width: 250
                        name: "Y"
                        minLabelWidth: 0
                    }
                }

                Grid {
                    columns: 2
                    spacing: 2
                    Examples.Button {
                        text: "flick"
                        onClicked: flick.flick(xvelSlider.value, yvelSlider.value)
                        width: zeroButton.width
                    }
                    Examples.Button {
                        text: "cancel"
                        onClicked: flick.cancelFlick()
                        width: zeroButton.width
                    }
                    Examples.Button {
                        id: zeroButton
                        text: "<- zero"
                        onClicked: {
                            xvelSlider.setValue(5000)
                            yvelSlider.setValue(5000)
                        }
                    }
                    Examples.Button {
                        text: "home"
                        width: zeroButton.width
                        onClicked: {
                            flick.contentX = 0
                            flick.contentY = 0
                        }
                    }
                }
            }
        }

        Column {
            height: parent.height
            width: movingLabel.implicitWidth * 1.5
            spacing: 2
            Text {
                id: movingLabel
                text: "moving:"
                color: flick.moving ? "green" : "black"
            }
            Rectangle {
                width: parent.width
                height: hVelLabel.implicitHeight + 4
                color: flick.movingHorizontally ? "green" : "darkgrey"
                Text {
                    id: hVelLabel
                    anchors.centerIn: parent
                    color: "white"
                    text: "h " + flick.horizontalVelocity.toFixed(2)
                }
            }
            Rectangle {
                width: parent.width
                height: vVelLabel.implicitHeight + 4
                color: flick.movingVertically ? "green" : "darkgrey"
                Text {
                    id: vVelLabel
                    anchors.centerIn: parent
                    color: "white"
                    text: "v " + flick.verticalVelocity.toFixed(2)
                }
            }
        }

        Column {
            height: parent.height
            width: draggingLabel.implicitWidth
            spacing: 2
            Text {
                id: draggingLabel
                text: "dragging:"
                color: flick.dragging ? "green" : "black"
            }
            Rectangle {
                width: draggingLabel.implicitWidth
                height: hVelLabel.implicitHeight + 4
                color: flick.draggingHorizontally ? "green" : "darkgrey"
                Text {
                    anchors.centerIn: parent
                    color: "white"
                    text: "h"
                }
            }
            Rectangle {
                width: draggingLabel.implicitWidth
                height: vVelLabel.implicitHeight + 4
                color: flick.draggingVertically ? "green" : "darkgrey"
                Text {
                    anchors.centerIn: parent
                    color: "white"
                    text: "v"
                }
            }
        }

        Column {
            height: parent.height
            width: flickingLabel.implicitWidth
            spacing: 2
            Text {
                id: flickingLabel
                text: "flicking:"
                color: flick.flicking ? "green" : "black"
            }
            Rectangle {
                width: flickingLabel.implicitWidth
                height: hVelLabel.implicitHeight + 4
                color: flick.flickingHorizontally ? "green" : "darkgrey"
                Text {
                    anchors.centerIn: parent
                    color: "white"
                    text: "h"
                }
            }
            Rectangle {
                width: flickingLabel.implicitWidth
                height: vVelLabel.implicitHeight + 4
                color: flick.flickingVertically ? "green" : "darkgrey"
                Text {
                    anchors.centerIn: parent
                    color: "white"
                    text: "v"
                }
            }
        }

        Column {
            Examples.CheckBox {
                id: pxAlignCB
                text: "pixel aligned"
            }
            Text {
                text: "content X " + flick.contentX.toFixed(2) + " Y " + flick.contentY.toFixed(2)
            }
        }
    }

    Component.onCompleted: {
        var request = new XMLHttpRequest()
        request.open('GET', 'qrc:/flicktext.qml')
        request.onreadystatechange = function(event) {
            if (request.readyState === XMLHttpRequest.DONE)
                text.text = request.responseText
        }
        request.send()
    }
}
