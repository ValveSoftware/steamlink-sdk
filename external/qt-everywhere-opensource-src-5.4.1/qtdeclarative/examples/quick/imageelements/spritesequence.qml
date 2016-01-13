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

Item {
    width: 320
    height: 480
    MouseArea {
        onClicked: anim.start();
        anchors.fill: parent
    }
//! [animation]
    SequentialAnimation {
        id: anim
        ScriptAction { script: image.goalSprite = "falling"; }
        NumberAnimation { target: image; property: "y"; to: 480; duration: 12000; }
        ScriptAction { script: {image.goalSprite = ""; image.jumpTo("still");} }
        PropertyAction { target: image; property: "y"; value: 0 }
    }
//! [animation]
    SpriteSequence {
        id: image
        width: 256
        height: 256
        anchors.horizontalCenter: parent.horizontalCenter
        interpolate: false
        goalSprite: ""
//! [still]
        Sprite{
            name: "still"
            source: "content/BearSheet.png"
            frameCount: 1
            frameWidth: 256
            frameHeight: 256
            frameDuration: 100
            to: {"still":1, "blink":0.1, "floating":0}
        }
//! [still]
        Sprite{
            name: "blink"
            source: "content/BearSheet.png"
            frameCount: 3
            frameX: 256
            frameY: 1536
            frameWidth: 256
            frameHeight: 256
            frameDuration: 100
            to: {"still":1}
        }
        Sprite{
            name: "floating"
            source: "content/BearSheet.png"
            frameCount: 9
            frameX: 0
            frameY: 0
            frameWidth: 256
            frameHeight: 256
            frameDuration: 160
            to: {"still":0, "flailing":1}
        }
        Sprite{
            name: "flailing"
            source: "content/BearSheet.png"
            frameCount: 8
            frameX: 0
            frameY: 768
            frameWidth: 256
            frameHeight: 256
            frameDuration: 160
            to: {"falling":1}
        }
        Sprite{
            name: "falling"
            source: "content/BearSheet.png"
            frameCount: 5
            frameY: 1280
            frameWidth: 256
            frameHeight: 256
            frameDuration: 160
            to: {"falling":1}
        }
    }
}
