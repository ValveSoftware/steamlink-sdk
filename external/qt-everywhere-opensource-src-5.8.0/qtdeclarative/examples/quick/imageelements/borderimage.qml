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
import "content"

Rectangle {
    id: page
    width: 320
    height: 480

    BorderImageSelector {
        id: selector
        curIdx: 0
        maxIdx: 3
        gridWidth: 240
        flickable: mainFlickable
        width: parent.width
        height: 64
    }

    Flickable {
        id: mainFlickable
        width: parent.width
        anchors.bottom: parent.bottom
        anchors.top: selector.bottom
        interactive: false //Animated through selector control
        contentX: -120
        Behavior on contentX { NumberAnimation {}}
        contentWidth: 1030
        contentHeight: 420
        Grid {
            anchors.centerIn: parent; spacing: 20

            MyBorderImage {
                minWidth: 120; maxWidth: 240; minHeight: 120; maxHeight: 200
                source: "content/colors.png"; margin: 30
            }

            MyBorderImage {
                minWidth: 120; maxWidth: 240; minHeight: 120; maxHeight: 200
                source: "content/colors.png"; margin: 30
                horizontalMode: BorderImage.Repeat; verticalMode: BorderImage.Repeat
            }

            MyBorderImage {
                minWidth: 120; maxWidth: 240; minHeight: 120; maxHeight: 200
                source: "content/colors.png"; margin: 30
                horizontalMode: BorderImage.Stretch; verticalMode: BorderImage.Repeat
            }

            MyBorderImage {
                minWidth: 120; maxWidth: 240; minHeight: 120; maxHeight: 200
                source: "content/colors.png"; margin: 30
                horizontalMode: BorderImage.Round; verticalMode: BorderImage.Round
            }

            MyBorderImage {
                minWidth: 60; maxWidth: 200; minHeight: 40; maxHeight: 200
                source: "content/bw.png"; margin: 10
            }

            MyBorderImage {
                minWidth: 60; maxWidth: 200; minHeight: 40; maxHeight: 200
                source: "content/bw.png"; margin: 10
                horizontalMode: BorderImage.Repeat; verticalMode: BorderImage.Repeat
            }

            MyBorderImage {
                minWidth: 60; maxWidth: 200; minHeight: 40; maxHeight: 200
                source: "content/bw.png"; margin: 10
                horizontalMode: BorderImage.Stretch; verticalMode: BorderImage.Repeat
            }

            MyBorderImage {
                minWidth: 60; maxWidth: 200; minHeight: 40; maxHeight: 200
                source: "content/bw.png"; margin: 10
                horizontalMode: BorderImage.Round; verticalMode: BorderImage.Round
            }
        }
    }
}
