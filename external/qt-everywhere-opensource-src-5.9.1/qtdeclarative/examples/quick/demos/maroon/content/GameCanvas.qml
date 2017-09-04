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
import "logic.js" as Logic
import "towers" as Towers

Item {
    id: grid

    property int squareSize: 64
    property int rows: 6
    property int cols: 4
    property Item canvas: grid
    property int score: 0
    property int coins: 100
    property int lives: 3
    property int waveNumber: 0
    property int waveProgress: 0
    property var towers
    property var mobs
    property bool gameRunning: false
    property bool gameOver: false
    property bool errored: false
    property string errorString: ""

    width: cols * squareSize
    height: rows * squareSize

    function freshState() {
        lives = 3
        coins = 100
        score = 0
        waveNumber = 0
        waveProgress = 0
        gameOver = false
        gameRunning = false
        towerMenu.shown = false
        helpButton.comeBack();
    }

    Text {
        id: errorText // Mostly for debug purposes
        text: errorString
        visible: errored
        color: "red"
        font.pixelSize: 18
        wrapMode: Text.WordWrap
        width: parent.width / 1.2
        height: parent.height / 1.2
        anchors.centerIn: parent
        z: 1000
    }

    Timer {
        interval: 16
        running: true
        repeat: true
        onTriggered: Logic.tick()
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        onClicked: {
            if (towerMenu.visible)
                towerMenu.finish()
            else
                towerMenu.open(mouse.x, mouse.y)
        }
    }

    Image {
        id: towerMenu
        visible: false
        z: 1500
        scale: 0.9
        opacity: 0.7
        property int dragDistance: 16
        property int targetRow: 0
        property int targetCol: 0
        property bool shown: false
        property bool towerExists: false

        function finish() {
            shown = false
        }

        function open(xp,yp) {
            if (!grid.gameRunning)
                return
            targetRow = Logic.row(yp)
            targetCol = Logic.col(xp)
            if (targetRow == 0)
                towerMenu.y = (targetRow + 1) * grid.squareSize
            else
                towerMenu.y = (targetRow - 1) * grid.squareSize
            towerExists = (grid.towers[Logic.towerIdx(targetCol, targetRow)] != null)
            shown = true
            helpButton.goAway();
        }

        states: State {
            name: "shown"; when: towerMenu.shown && !grid.gameOver
            PropertyChanges { target: towerMenu; visible: true; scale: 1; opacity: 1 }
        }

        transitions: Transition {
            PropertyAction { property: "visible" }
            NumberAnimation { properties: "opacity,scale"; duration: 500; easing.type: Easing.OutElastic }
        }

        x: -32
        source: "gfx/dialog.png"
        Row {
            id: buttonRow
            height: 100
            anchors.centerIn: parent
            spacing: 8
            BuildButton {
                row: towerMenu.targetRow; col: towerMenu.targetCol
                anchors.verticalCenter: parent.verticalCenter
                towerType: 1; index: 0
                canBuild: !towerMenu.towerExists
                source: "gfx/dialog-melee.png"
                onClicked: towerMenu.finish()
            }
            BuildButton {
                row: towerMenu.targetRow; col: towerMenu.targetCol
                anchors.verticalCenter: parent.verticalCenter
                towerType: 2; index: 1
                canBuild: !towerMenu.towerExists
                source: "gfx/dialog-shooter.png"
                onClicked: towerMenu.finish()
            }
            BuildButton {
                row: towerMenu.targetRow; col: towerMenu.targetCol
                anchors.verticalCenter: parent.verticalCenter
                towerType: 3; index: 2
                canBuild: !towerMenu.towerExists
                source: "gfx/dialog-bomb.png"
                onClicked: towerMenu.finish()
            }
            BuildButton {
                row: towerMenu.targetRow; col: towerMenu.targetCol
                anchors.verticalCenter: parent.verticalCenter
                towerType: 4; index: 3
                canBuild: !towerMenu.towerExists
                source: "gfx/dialog-factory.png"
                onClicked: towerMenu.finish()
            }
        }
    }


    Keys.onPressed: { // Cheat Codes while Testing
        if (event.key == Qt.Key_Up && (event.modifiers & Qt.ShiftModifier))
            grid.coins += 10;
        if (event.key == Qt.Key_Left && (event.modifiers & Qt.ShiftModifier))
            grid.lives += 1;
        if (event.key == Qt.Key_Down && (event.modifiers & Qt.ShiftModifier))
            Logic.gameState.waveProgress += 1000;
        if (event.key == Qt.Key_Right && (event.modifiers & Qt.ShiftModifier))
            Logic.endGame();
    }

    Image {
        id: helpButton
        z: 1010
        source: "gfx/button-help.png"
        function goAway() {
            helpMA.enabled = false;
            helpButton.opacity = 0;
        }
        function comeBack() {
            helpMA.enabled = true;
            helpButton.opacity = 1;
        }
        Behavior on opacity { NumberAnimation {} }
        MouseArea {
            id: helpMA
            anchors.fill: parent
            onClicked: {helpImage.visible = true; helpButton.visible = false;}
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
    }

    Image {
        id: helpImage
        z: 1010
        source: "gfx/help.png"
        anchors.fill: parent
        visible: false
        MouseArea {
            anchors.fill: parent
            onClicked: helpImage.visible = false;
        }
    }

}
