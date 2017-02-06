/****************************************************************************
 **
 ** Copyright (C) 2015 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
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
import QtMultimedia 5.0

Rectangle {
    width: 400; height: 300;

    Radio {
        id: radio
        band: Radio.FM
    }

    Column {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        Row {

            Text {
                id: freq

                width: 150
                height: 200

                verticalAlignment: Text.AlignVCenter
                text: "" + radio.frequency / 1000 + " kHz"
            }
            Text {
                id: sig

                width: 200
                height: 200

                verticalAlignment: Text.AlignVCenter
                text: (radio.availability == Radio.Available ? "No Signal " : "No Radio Found")
            }
        }

        Row {
            spacing: 5

            Rectangle {
                width: 350
                height: 10

                color: "black"

                Rectangle {
                    width: 5
                    height: 10
                    color: "red"

                    y: 0
                    x: (parent.width - 5) * ((radio.frequency - radio.minimumFrequency) / (radio.maximumFrequency -
                    radio.minimumFrequency))

                }
            }
        }


        Row {
            spacing: 5

            Rectangle {
                id: scanDownButton
                border.color: "black"
                border.width: 1
                radius: 2

                width: 90
                height: 40

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "Scan Down"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: radio.scanDown();
                }
            }
            Rectangle {
                id: freqDownButton
                border.color: "black"
                border.width: 1
                radius: 2

                width: 90
                height: 40

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "Freq Down"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: radio.tuneDown();
                }
            }
            Rectangle {
                id: freqUpButton
                border.color: "black"
                border.width: 1
                radius: 2

                width: 90
                height: 40

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "Freq Up"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: radio.tuneUp();
                }
            }
            Rectangle {
                id: scanUpButton
                border.color: "black"
                border.width: 1
                radius: 2

                width: 90
                height: 40

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: "Scan Up"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: radio.scanUp();
                }
            }
        }
    }
}
