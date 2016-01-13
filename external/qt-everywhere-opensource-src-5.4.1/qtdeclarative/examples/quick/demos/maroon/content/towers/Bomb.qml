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
    hp: 10
    range: 0.4
    rof: 10
    property real detonationRange: 2.5

    function fire() {
        sound.play()
        sprite.jumpTo("shoot")
        animDelay.start()
    }

    function finishFire() {
        var sCol = Math.max(0, col - 1)
        var eCol = Math.min(Logic.gameState.cols - 1, col + 1)
        var killList = new Array()
        for (var i = sCol; i <= eCol; i++) {
            for (var j = 0; j < Logic.gameState.mobs[i].length; j++)
                if (Math.abs(Logic.gameState.mobs[i][j].y - container.y) < Logic.gameState.squareSize * detonationRange)
                    killList.push(Logic.gameState.mobs[i][j])
            while (killList.length > 0)
                Logic.killMob(i, killList.pop())
        }
        Logic.killTower(row, col);
    }

    Timer {
        id: animDelay
        running: false
        interval: shootState.frameCount * shootState.frameDuration
        onTriggered: finishFire()
    }

    function die()
    {
        destroy() // No blink, because we usually meant to die
    }

    SoundEffect {
        id: sound
        source: "../audio/bomb-action.wav"
    }

    SpriteSequence {
        id: sprite
        width: 64
        height: 64
        interpolate: false
        goalSprite: ""

        Sprite {
            name: "idle"
            source: "../gfx/bomb-idle.png"
            frameCount: 4
            frameDuration: 800
        }

        Sprite {
            id: shootState
            name: "shoot"
            source: "../gfx/bomb-action.png"
            frameCount: 6
            frameDuration: 155
            to: { "dying" : 1 } // So that if it takes a frame to clean up, it is on the last frame of the explosion
        }

        Sprite {
            name: "dying"
            source: "../gfx/bomb-action.png"
            frameCount: 1
            frameX: 64 * 5
            frameWidth: 64
            frameHeight: 64
            frameDuration: 155
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
}
