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
    hp: 2
    range: 6
    damage: 0 // By projectile
    rof: 40
    income: 0
    property var targetMob
    property real realDamage: 1
    function fire() {
        proj.x = 32 - proj.width / 2
        proj.y = 0
        targetMob = Logic.gameState.mobs[col][0]
        projAnim.to = targetMob.y - container.y -10
        projAnim.start()
        shootSound.play()
        sprite.jumpTo("shoot")
    }

    Image {
        id: proj
        y: 1000
        SequentialAnimation on y {
            id: projAnim
            running: false
            property real to: 1000
            SmoothedAnimation {
                to: projAnim.to
                velocity: 400
            }
            ScriptAction {
                script: {
                    if (targetMob && targetMob.hit) {
                        targetMob.hit(realDamage)
                        targetMob.inked()
                        projSound.play()
                    }
                }
            }
            PropertyAction {
                value: 1000;
            }
        }
        source: "../gfx/projectile.png"
    }

    SoundEffect {
        id: shootSound
        source: "../audio/shooter-action.wav"
    }
    SoundEffect {
        id: projSound
        source: "../audio/projectile-action.wav"
    }

    SpriteSequence {
        id: sprite
        width: 64
        height: 64
        interpolate: false
        goalSprite: ""

        Sprite {
            name: "idle"
            source: "../gfx/shooter-idle.png"
            frameCount: 4
            frameDuration: 250
        }

        Sprite {
            name: "shoot"
            source: "../gfx/shooter-action.png"
            frameCount: 5
            frameDuration: 90
            to: { "idle" : 1 }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: x; to: x - 4; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { from: x - 4; to: x; duration: 900; easing.type: Easing.InOutQuad }
        }
    }
}
