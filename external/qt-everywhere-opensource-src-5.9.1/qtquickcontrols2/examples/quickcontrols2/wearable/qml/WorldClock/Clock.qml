/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.7
import "../Style"

Item {
    id: clock

    property int hours
    property int minutes
    property int seconds
    property real shift: timeShift
    property bool night: false
    property bool internationalTime: true //Unset for local time

    function getWatchFaceImage(imageName) {
        return "images/" + imageName
    }

    function timeChanged() {
        var date = new Date
        hours = internationalTime ? date.getUTCHours() + Math.floor(
                                        clock.shift) : date.getHours()
        night = (hours < 7 || hours > 19)
        minutes = internationalTime ?
         date.getUTCMinutes() + ((clock.shift % 1) * 60) : date.getMinutes()
        seconds = date.getUTCSeconds()
    }

    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: clock.timeChanged()
    }

    Item {
        anchors.centerIn: parent

        width: 200
        height: 220

        Rectangle {
            color: clock.night ? UIStyle.colorQtGray1 : UIStyle.colorQtGray10
            radius: width / 2
            width: parent.width
            height: parent.width
        }

        Image {
            id: background
            source: getWatchFaceImage("swissdaydial.png")
            visible: clock.night == false
        }
        Image {
            source: getWatchFaceImage("swissnightdial.png")
            visible: clock.night == true
        }

        Image {
            x: 92.5
            y: 27
            source: getWatchFaceImage(clock.night ?
                                        "swissnighthour.png"
                                        : "swissdayhour.png")
            transform: Rotation {
                id: hourRotation
                origin.x: 7.5
                origin.y: 73
                angle: (clock.hours * 30) + (clock.minutes * 0.5)
                Behavior on angle {
                    SpringAnimation {
                        spring: 2
                        damping: 0.2
                        modulus: 360
                    }
                }
            }
        }

        Image {
            x: 93.5
            y: 17
            source: getWatchFaceImage(clock.night ?
                                        "swissnightminute.png"
                                        : "swissdayminute.png")
            transform: Rotation {
                id: minuteRotation
                origin.x: 6.5
                origin.y: 83
                angle: clock.minutes * 6
                Behavior on angle {
                    SpringAnimation {
                        spring: 2
                        damping: 0.2
                        modulus: 360
                    }
                }
            }
        }

        Image {
            x: 97.5
            y: 20
            source: getWatchFaceImage("second.png")
            transform: Rotation {
                id: secondRotation
                origin.x: 2.5
                origin.y: 80
                angle: clock.seconds * 6
                Behavior on angle {
                    SpringAnimation {
                        spring: 2
                        damping: 0.2
                        modulus: 360
                    }
                }
            }
        }

        Image {
            anchors.centerIn: background
            source: getWatchFaceImage("center.png")
        }

        Text {
            id: cityLabel
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.horizontalCenter: parent.horizontalCenter

            text: cityName
            color: UIStyle.colorQtGray1
            font.pixelSize: UIStyle.fontSizeXS
            font.letterSpacing: 2
        }
    }
}
