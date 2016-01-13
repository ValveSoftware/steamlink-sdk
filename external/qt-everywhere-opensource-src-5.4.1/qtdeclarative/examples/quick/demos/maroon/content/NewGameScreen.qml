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

// This is the first screen.
// It shows the logo and emit a startButtonClicked signal
// when the user press the "PLAY" button.

Item {
    id: newGameScreen
    width: 320
    height: 480

    signal startButtonClicked

    Image {
        source: "gfx/logo.png"
        anchors.top: parent.top
        anchors.topMargin: 60
    }

    Image {
        source: "gfx/logo-fish.png"
        anchors.top: parent.top

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: x + 148; to: x + 25; duration: 2000; easing.type: Easing.InOutQuad }
            NumberAnimation { from: x + 25; to: x + 148; duration: 1600; easing.type: Easing.InOutQuad }
        }
        SequentialAnimation on anchors.topMargin {
            loops: Animation.Infinite
            NumberAnimation { from: 100; to: 60; duration: 1600; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 60; to: 100; duration: 2000; easing.type: Easing.InOutQuad }
        }
    }

    Image {
        source: "gfx/logo-bubble.png"
        anchors.top: parent.top

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: x + 140; to: x + 40; duration: 2000; easing.type: Easing.InOutQuad }
            NumberAnimation { from: x + 40; to: x + 140; duration: 1600; easing.type: Easing.InOutQuad }
        }
        SequentialAnimation on anchors.topMargin {
            loops: Animation.Infinite
            NumberAnimation { from: 100; to: 60; duration: 1600; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 60; to: 100; duration: 2000; easing.type: Easing.InOutQuad }
        }
        SequentialAnimation on width {
            loops: Animation.Infinite
            NumberAnimation { from: 140; to: 160; duration: 1000; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 160; to: 140; duration: 800; easing.type: Easing.InOutQuad }
        }
        SequentialAnimation on height {
            loops: Animation.Infinite
            NumberAnimation { from: 150; to: 140; duration: 800; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 140; to: 150; duration: 1000; easing.type: Easing.InOutQuad }
        }
    }

    Image {
        source: "gfx/button-play.png"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        MouseArea {
            anchors.fill: parent
            onClicked: newGameScreen.startButtonClicked()
        }
    }
}
