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
import "../logic.js" as Logic
import ".."

TowerBase {
    id: container
    rof: 160
    income: 5
    SpriteSequence {
        id: sprite
        width: 64
        height: 64
        interpolate: false
        goalSprite: ""

        Sprite {
            name: "idle"
            source: "../gfx/factory-idle.png"
            frameCount: 4
            frameDuration: 200
        }

        Sprite {
            name: "action"
            source: "../gfx/factory-action.png"
            frameCount: 4
            frameDuration: 90
            to: { "idle" : 1 }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: x; to: x + 4; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { from: x + 4; to: x; duration: 900; easing.type: Easing.InOutQuad }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: y; to: y - 4; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { from: y - 4; to: y; duration: 900; easing.type: Easing.InOutQuad }
        }
    }

    SoundEffect {
        id: actionSound
        source: "../audio/factory-action.wav"
    }

    function fire() {
        actionSound.play()
        sprite.jumpTo("action")
        coinLaunch.start()
    }

    function spawn() {
        coin.target = Logic.gameState.mapToItem(container, 240, -32)
    }

    Image {
        id: coin
        property var target: { "x" : 0, "y" : 0 }
        source: "../gfx/currency.png"
        visible: false
    }

    SequentialAnimation {
        id: coinLaunch
        PropertyAction { target: coin; property: "visible"; value: true }
        ParallelAnimation {
            NumberAnimation { target: coin; property: "x"; from: 16; to: coin.target.x }
            NumberAnimation { target: coin; property: "y"; from: 16; to: coin.target.y }
        }
        PropertyAction { target: coin; property: "visible"; value: false }
    }
}
