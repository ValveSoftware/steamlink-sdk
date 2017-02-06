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
import QtQuick.Particles 2.0
import "samegame.js" as Logic
import "."

Item {
    id: gameCanvas
    property bool gameOver: true
    property int score: 0
    property int highScore: 0
    property int moves: 0
    property string mode: ""
    property ParticleSystem ps: particleSystem
    //For easy theming
    property alias backgroundVisible: bg.visible
    property string background: "gfx/background.png"
    property string blockFile: "Block.qml"
    property int blockSize: Settings.blockSize
    onBlockFileChanged: Logic.changeBlock(blockFile);
    property alias particlePack: auxLoader.source
    //For multiplayer
    property int score2: 0
    property int curTurn: 1
    property bool autoTurnChange: false
    signal swapPlayers
    property bool swapping: false
    //onSwapPlayers: if (autoTurnChange) Logic.turnChange();//Now implemented below
    //For puzzle
    property url level
    property bool puzzleWon: false
    signal puzzleLost //Since root is tracking the puzzle progress
    function showPuzzleEnd (won) {
        if (won) {
            smokeParticle.color = Qt.rgba(0,1,0,0);
            puzzleWin.play();
        } else {
            smokeParticle.color = Qt.rgba(1,0,0,0);
            puzzleFail.play();
            puzzleLost();
        }
    }
    function showPuzzleGoal (str) {
        puzzleTextBubble.opacity = 1;
        puzzleTextLabel.text = str;
    }
    Image {
        id: bg
        z: -1
        anchors.fill: parent
        source: background;
        fillMode: Image.PreserveAspectCrop
    }

    MouseArea {
        anchors.fill: parent; onClicked: {
            if (puzzleTextBubble.opacity == 1) {
                puzzleTextBubble.opacity = 0;
                Logic.finishLoadingMap();
            } else if (!swapping) {
                Logic.handleClick(mouse.x,mouse.y);
            }
        }
    }

    Image {
        id: highScoreTextBubble
        opacity: mode == "arcade" && gameOver && gameCanvas.score == gameCanvas.highScore ? 1 : 0
        Behavior on opacity { NumberAnimation {} }
        anchors.centerIn: parent
        z: 10
        source: "gfx/bubble-highscore.png"
        Image {
            anchors.centerIn: parent
            source: "gfx/text-highscore-new.png"
            rotation: -10
        }
    }

    Image {
        id: puzzleTextBubble
        anchors.centerIn: parent
        opacity: 0
        Behavior on opacity { NumberAnimation {} }
        z: 10
        source: "gfx/bubble-puzzle.png"
        Connections {
            target: gameCanvas
            onModeChanged: if (mode != "puzzle" && puzzleTextBubble.opacity > 0) puzzleTextBubble.opacity = 0;
        }
        Text {
            id: puzzleTextLabel
            width: parent.width - 24
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            color: "white"
            font.pixelSize: 24
            font.bold: true
            wrapMode: Text.WordWrap
        }
    }
    onModeChanged: {
        p1WonImg.opacity = 0;
        p2WonImg.opacity = 0;
    }
    SmokeText { id: puzzleWin; source: "gfx/icon-ok.png"; system: particleSystem }
    SmokeText { id: puzzleFail; source: "gfx/icon-fail.png"; system: particleSystem }

    onSwapPlayers: {
        smokeParticle.color = "yellow"
        Logic.turnChange();
        if (curTurn == 1) {
            p1Text.play();
        } else {
            p2Text.play();
        }
        clickDelay.running = true;
    }
    SequentialAnimation {
        id: clickDelay
        ScriptAction { script: gameCanvas.swapping = true; }
        PauseAnimation { duration: 750 }
        ScriptAction { script: gameCanvas.swapping = false; }
    }

    SmokeText {
        id: p1Text; source: "gfx/text-p1-go.png";
        system: particleSystem; playerNum: 1
        opacity: p1WonImg.opacity + p2WonImg.opacity > 0 ? 0 : 1
    }

    SmokeText {
        id: p2Text; source: "gfx/text-p2-go.png";
        system: particleSystem; playerNum: 2
        opacity: p1WonImg.opacity + p2WonImg.opacity > 0 ? 0 : 1
    }

    onGameOverChanged: {
        if (gameCanvas.mode == "multiplayer") {
            if (gameCanvas.score >= gameCanvas.score2) {
                p1WonImg.opacity = 1;
            } else {
                p2WonImg.opacity = 1;
            }
        }
    }
    Image {
        id: p1WonImg
        source: "gfx/text-p1-won.png"
        anchors.centerIn: parent
        opacity: 0
        Behavior on opacity { NumberAnimation {} }
        z: 10
    }
    Image {
        id: p2WonImg
        source: "gfx/text-p2-won.png"
        anchors.centerIn: parent
        opacity: 0
        Behavior on opacity { NumberAnimation {} }
        z: 10
    }

    ParticleSystem{
        id: particleSystem;
        anchors.fill: parent
        z: 5
        ImageParticle {
            id: smokeParticle
            groups: ["smoke"]
            source: "gfx/particle-smoke.png"
            alpha: 0.1
            alphaVariation: 0.1
            color: "yellow"
        }
        Loader {
            id: auxLoader
            anchors.fill: parent
            source: "PrimaryPack.qml"
            onItemChanged: {
                if (item && "particleSystem" in item)
                    item.particleSystem = particleSystem
                if (item && "gameArea" in item)
                    item.gameArea = gameCanvas
            }
        }
    }
}

