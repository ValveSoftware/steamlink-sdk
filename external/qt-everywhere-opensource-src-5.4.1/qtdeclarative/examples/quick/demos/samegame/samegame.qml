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
import QtQuick.Particles 2.0
import "content/samegame.js" as Logic
import "content"

Rectangle {
    id: root
    width: Settings.screenWidth; height: Settings.screenHeight
    property int acc: 0


    function loadPuzzle() {
        if (gameCanvas.mode != "")
            Logic.cleanUp();
        Logic.startNewGame(gameCanvas,"puzzle","levels/level"+acc+".qml")
    }
    function nextPuzzle() {
        acc = (acc + 1) % 10;
        loadPuzzle();
    }
    Timer {
        id: gameOverTimer
        interval: 1500
        running : gameCanvas.gameOver && gameCanvas.mode == "puzzle" //mode will be reset by cleanUp();
        repeat  : false
        onTriggered: {
            Logic.cleanUp();
            nextPuzzle();
        }
    }

    Image {
        source: "content/gfx/background.png"
        anchors.fill: parent
    }

    GameArea {
        id: gameCanvas
        z: 1
        y: Settings.headerHeight

        width: parent.width
        height: parent.height - Settings.headerHeight - Settings.footerHeight

        backgroundVisible: root.state == "in-game"
        onModeChanged: if (gameCanvas.mode != "puzzle") puzzleWon = false; //UI has stricter constraints on this variable than the game does
        Age {
            groups: ["redspots", "greenspots", "bluespots", "yellowspots"]
            enabled: root.state == ""
            system: gameCanvas.ps
        }

        onPuzzleLost: acc--;//So that nextPuzzle() reloads the current one

    }

    Item {
        id: menu
        z: 2
        width: parent.width;
        anchors.top: parent.top
        anchors.bottom: bottomBar.top

        LogoAnimation {
            x: 64
            y: Settings.headerHeight
            particleSystem: gameCanvas.ps
            running: root.state == ""
        }
        Row {
            x: 112
            y: 20
            Image { source: "content/gfx/logo-a.png" }
            Image { source: "content/gfx/logo-m.png" }
            Image { source: "content/gfx/logo-e.png" }
        }

        Column {
            y: 100 + 40
            spacing: Settings.menuButtonSpacing
            width: parent.width
            height: parent.height - (140 + Settings.footerHeight)

            Button {
                width: root.width
                rotatedButton: true
                imgSrc: "content/gfx/but-game-1.png"
                onClicked: {
                    if (root.state == "in-game")
                        return //Prevent double clicking
                    root.state = "in-game"
                    gameCanvas.blockFile = "Block.qml"
                    gameCanvas.background = "gfx/background.png"
                    arcadeTimer.start();
                }
                //Emitted particles don't fade out, because ImageParticle is on the GameArea
                system: gameCanvas.ps
                group: "green"
                Timer {
                    id: arcadeTimer
                    interval: Settings.menuDelay
                    running : false
                    repeat  : false
                    onTriggered: Logic.startNewGame(gameCanvas)
                }
            }

            Button {
                width: root.width
                rotatedButton: true
                imgSrc: "content/gfx/but-game-2.png"
                onClicked: {
                    if (root.state == "in-game")
                        return
                    root.state = "in-game"
                    gameCanvas.blockFile = "Block.qml"
                    gameCanvas.background = "gfx/background.png"
                    twopTimer.start();
                }
                system: gameCanvas.ps
                group: "green"
                Timer {
                    id: twopTimer
                    interval: Settings.menuDelay
                    running : false
                    repeat  : false
                    onTriggered: Logic.startNewGame(gameCanvas, "multiplayer")
                }
            }

            Button {
                width: root.width
                rotatedButton: true
                imgSrc: "content/gfx/but-game-3.png"
                onClicked: {
                    if (root.state == "in-game")
                        return
                    root.state = "in-game"
                    gameCanvas.blockFile = "SimpleBlock.qml"
                    gameCanvas.background = "gfx/background.png"
                    endlessTimer.start();
                }
                system: gameCanvas.ps
                group: "blue"
                Timer {
                    id: endlessTimer
                    interval: Settings.menuDelay
                    running : false
                    repeat  : false
                    onTriggered: Logic.startNewGame(gameCanvas, "endless")
                }
            }

            Button {
                width: root.width
                rotatedButton: true
                imgSrc: "content/gfx/but-game-4.png"
                group: "yellow"
                onClicked: {
                    if (root.state == "in-game")
                        return
                    root.state = "in-game"
                    gameCanvas.blockFile = "PuzzleBlock.qml"
                    gameCanvas.background = "gfx/background.png"
                    puzzleTimer.start();
                }
                Timer {
                    id: puzzleTimer
                    interval: Settings.menuDelay
                    running : false
                    repeat  : false
                    onTriggered: loadPuzzle();
                }
                system: gameCanvas.ps
            }
        }
    }

    Image {
        id: scoreBar
        source: "content/gfx/bar.png"
        width: parent.width
        z: 6
        y: -Settings.headerHeight
        height: Settings.headerHeight
        Behavior on opacity { NumberAnimation {} }
        SamegameText {
            id: arcadeScore
            anchors { right: parent.right; topMargin: 3; rightMargin: 11; top: parent.top}
            text: '<font color="#f7d303">P1:</font> ' + gameCanvas.score
            font.pixelSize: Settings.fontPixelSize
            textFormat: Text.StyledText
            color: "white"
            opacity: gameCanvas.mode == "arcade" ? 1 : 0
            Behavior on opacity { NumberAnimation {} }
        }
        SamegameText {
            id: arcadeHighScore
            anchors { left: parent.left; topMargin: 3; leftMargin: 11; top: parent.top}
            text: '<font color="#f7d303">Highscore:</font> ' + gameCanvas.highScore
            opacity: gameCanvas.mode == "arcade" ? 1 : 0
        }
        SamegameText {
            id: p1Score
            anchors { right: parent.right; topMargin: 3; rightMargin: 11; top: parent.top}
            text: '<font color="#f7d303">P1:</font> ' + gameCanvas.score
            opacity: gameCanvas.mode == "multiplayer" ? 1 : 0
        }
        SamegameText {
            id: p2Score
            anchors { left: parent.left; topMargin: 3; leftMargin: 11; top: parent.top}
            text: '<font color="#f7d303">P2:</font> ' + gameCanvas.score2
            opacity: gameCanvas.mode == "multiplayer" ? 1 : 0
            rotation: 180
        }
        SamegameText {
            id: puzzleMoves
            anchors { left: parent.left; topMargin: 3; leftMargin: 11; top: parent.top}
            text: '<font color="#f7d303">Moves:</font> ' + gameCanvas.moves
            opacity: gameCanvas.mode == "puzzle" ? 1 : 0
        }
        SamegameText {
            Image {
                source: "content/gfx/icon-time.png"
                x: -20
            }
            id: puzzleTime
            anchors { topMargin: 3; top: parent.top; horizontalCenter: parent.horizontalCenter; horizontalCenterOffset: 20}
            text: "00:00"
            opacity: gameCanvas.mode == "puzzle" ? 1 : 0
            Timer {
                interval: 1000
                repeat: true
                running: gameCanvas.mode == "puzzle" && !gameCanvas.gameOver
                onTriggered: {
                    var elapsed = Math.floor((new Date() - Logic.gameDuration)/ 1000.0);
                    var mins = Math.floor(elapsed/60.0);
                    var secs = (elapsed % 60);
                    puzzleTime.text =  (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs;
                }
            }
        }
        SamegameText {
            id: puzzleScore
            anchors { right: parent.right; topMargin: 3; rightMargin: 11; top: parent.top}
            text: '<font color="#f7d303">Score:</font> ' + gameCanvas.score
            opacity: gameCanvas.mode == "puzzle" ? 1 : 0
        }
    }

    Image {
        id: bottomBar
        width: parent.width
        height: Settings.footerHeight
        source: "content/gfx/bar.png"
        y: parent.height - Settings.footerHeight;
        z: 2
        Button {
            id: quitButton
            height: Settings.toolButtonHeight
            imgSrc: "content/gfx/but-quit.png"
            onClicked: {Qt.quit(); }
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 11 }
        }
        Button {
            id: menuButton
            height: Settings.toolButtonHeight
            imgSrc: "content/gfx/but-menu.png"
            visible: (root.state == "in-game");
            onClicked: {root.state = ""; Logic.cleanUp(); gameCanvas.mode = ""}
            anchors { left: quitButton.right; verticalCenter: parent.verticalCenter; leftMargin: 0 }
        }
        Button {
            id: againButton
            height: Settings.toolButtonHeight
            imgSrc: "content/gfx/but-game-new.png"
            visible: (root.state == "in-game");
            opacity: gameCanvas.gameOver && (gameCanvas.mode == "arcade" || gameCanvas.mode == "multiplayer")
            Behavior on opacity{ NumberAnimation {} }
            onClicked: {if (gameCanvas.gameOver) { Logic.startNewGame(gameCanvas, gameCanvas.mode);}}
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 11 }
        }
        Button {
            id: nextButton
            height: Settings.toolButtonHeight
            imgSrc: "content/gfx/but-puzzle-next.png"
            visible: (root.state == "in-game") && gameCanvas.mode == "puzzle" && gameCanvas.puzzleWon
            opacity: gameCanvas.puzzleWon ? 1 : 0
            Behavior on opacity{ NumberAnimation {} }
            onClicked: {if (gameCanvas.puzzleWon) nextPuzzle();}
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 11 }
        }
    }

    Connections {
        target: root
        onStateChanged: stateChangeAnim.running = true
    }
    SequentialAnimation {
        id: stateChangeAnim
        ParallelAnimation {
            NumberAnimation { target: bottomBar; property: "y"; to: root.height; duration: Settings.menuDelay/2; easing.type: Easing.OutQuad }
            NumberAnimation { target: scoreBar; property: "y"; to: -Settings.headerHeight; duration: Settings.menuDelay/2; easing.type: Easing.OutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: bottomBar; property: "y"; to: root.height - Settings.footerHeight; duration: Settings.menuDelay/2; easing.type: Easing.OutBounce}
            NumberAnimation { target: scoreBar; property: "y"; to: root.state == "" ? -Settings.headerHeight : 0; duration: Settings.menuDelay/2; easing.type: Easing.OutBounce}
        }
    }

    states: [
        State {
            name: "in-game"
            PropertyChanges {
                target: menu
                opacity: 0
                visible: false
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {properties: "x,y,opacity"}
        }
    ]

    //"Debug mode"
    focus: true
    Keys.onAsteriskPressed: Logic.nuke();
    Keys.onSpacePressed: gameCanvas.puzzleWon = true;
}
