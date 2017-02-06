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

.pragma library // Shared game state
.import QtQuick 2.0 as QQ

// Game Stuff
var gameState // Local reference
function getGameState() { return gameState; }

var towerData = [ // Name and cost, stats are in the delegate per instance
    { "name": "Melee", "cost": 20 },
    { "name": "Ranged", "cost": 50 },
    { "name": "Bomb", "cost": 75 },
    { "name": "Factory", "cost": 25 }
]

var waveBaseData = [300, 290, 280, 270, 220, 180, 160, 80, 80, 80, 30, 30, 30, 30];
var waveData = [];

var towerComponents = new Array(towerData.length);
var mobComponent = Qt.createComponent("mobs/MobBase.qml");

function endGame()
{
    gameState.gameRunning = false;
    gameState.gameOver = true;
    for (var i = 0; i < gameState.cols; i++) {
        for (var j = 0; j < gameState.rows; j++) {
            if (gameState.towers[towerIdx(i, j)]) {
                gameState.towers[towerIdx(i, j)].destroy();
                gameState.towers[towerIdx(i, j)] = null;
            }
        }
        for (var j in gameState.mobs[i])
            gameState.mobs[i][j].destroy();
        gameState.mobs[i].splice(0,gameState.mobs[i].length); //Leaves queue reusable
    }
}

function startGame(gameCanvas)
{
    waveData = new Array();
    for (var i in waveBaseData)
        waveData[i] = waveBaseData[i];
    gameState.freshState();
    for (var i = 0; i < gameCanvas.cols; i++) {
        for (var j = 0; j < gameCanvas.rows; j++)
            gameState.towers[towerIdx(i, j)] = null;
        gameState.mobs[i] = new Array();
    }
    gameState.towers[towerIdx(0, 0)] = newTower(3, 0, 0);//Start with a starfish in the corner
    gameState.gameRunning = true;
    gameState.gameOver = false;
}

function newGameState(gameCanvas)
{
    for (var i = 0; i < towerComponents.length; i++) {
        towerComponents[i] = Qt.createComponent("towers/" + towerData[i].name + ".qml");
        if (towerComponents[i].status == QQ.Component.Error) {
            gameCanvas.errored = true;
            gameCanvas.errorString += "Loading Tower " + towerData[i].name + "\n" + (towerComponents[i].errorString());
            console.log(towerComponents[i].errorString());
        }
    }
    gameState = gameCanvas;
    gameState.freshState();
    gameState.towers = new Array(gameCanvas.rows * gameCanvas.cols);
    gameState.mobs = new Array(gameCanvas.cols);
    return gameState;
}

function row(y)
{
    return Math.floor(y / gameState.squareSize);
}

function col(x)
{
    return Math.floor(x / gameState.squareSize);
}

function towerIdx(x, y)
{
    return y + (x * gameState.rows);
}

function newMob(col)
{
    var ret = mobComponent.createObject(gameState.canvas,
        { "col" : col,
          "speed" : (Math.min(2.0, 0.10 * (gameState.waveNumber + 1))),
          "y" : gameState.canvas.height });
    gameState.mobs[col].push(ret);
    return ret;
}

function newTower(type, row, col)
{
    var ret = towerComponents[type].createObject(gameState.canvas);
    ret.row = row;
    ret.col = col;
    ret.fireCounter = ret.rof;
    ret.spawn();
    return ret;
}

function buildTower(type, x, y)
{
    if (gameState.towers[towerIdx(x,y)] != null) {
        if (type <= 0) {
            gameState.towers[towerIdx(x,y)].sell();
            gameState.towers[towerIdx(x,y)] = null;
        }
    } else {
        if (gameState.coins < towerData[type - 1].cost)
            return;
        gameState.towers[towerIdx(x, y)] = newTower(type - 1, y, x);
        gameState.coins -= towerData[type - 1].cost;
    }
}

function killMob(col, mob)
{
    if (!mob)
        return;
    var idx = gameState.mobs[col].indexOf(mob);
    if (idx == -1 || !mob.hp)
        return;
    mob.hp = 0;
    mob.die();
    gameState.mobs[col].splice(idx,1);
}

function killTower(row, col)
{
    var tower = gameState.towers[towerIdx(col, row)];
    if (!tower)
        return;
    tower.hp = 0;
    tower.die();
    gameState.towers[towerIdx(col, row)] = null;
}

function tick()
{
    if (!gameState.gameRunning)
        return;

    // Spawn
    gameState.waveProgress += 1;
    var i = gameState.waveProgress;
    var j = 0;
    while (i > 0 && j < waveData.length)
        i -= waveData[j++];
    if ( i == 0 ) // Spawn a mob
        newMob(Math.floor(Math.random() * gameState.cols));
    if ( j == waveData.length ) { // Next Wave
        gameState.waveNumber += 1;
        gameState.waveProgress = 0;
        var waveModifier = 10; // Constant governing how much faster the next wave is to spawn (not fish speed)
        for (var k in waveData ) // Slightly faster
            if (waveData[k] > waveModifier)
                waveData[k] -= waveModifier;
    }

    // Towers Attack
    for (var j in gameState.towers) {
        var tower = gameState.towers[j];
        if (tower == null)
            continue;
        if (tower.fireCounter > 0) {
            tower.fireCounter -= 1;
            continue;
        }
        var column = tower.col;
        for (var k in gameState.mobs[column]) {
            var conflict = gameState.mobs[column][k];
            if (conflict.y <= gameState.canvas.height && conflict.y + conflict.height > tower.y
                && conflict.y - ((tower.row + 1) * gameState.squareSize) < gameState.squareSize * tower.range) { // In Range
                tower.fire();
                tower.fireCounter = tower.rof;
                conflict.hit(tower.damage);
            }
        }

        // Income
        if (tower.income) {
            gameState.coins += tower.income;
            tower.fire();
            tower.fireCounter = tower.rof;
        }
    }

    // Mobs move
    for (var i = 0; i < gameState.cols; i++) {
        for (var j = 0; j < gameState.mobs[i].length; j++) {
            var mob = gameState.mobs[i][j];
            var newPos = gameState.mobs[i][j].y - gameState.mobs[i][j].speed;
            if (newPos < 0) {
                gameState.lives -= 1;
                killMob(i, mob);
                if (gameState.lives <= 0)
                    endGame();
                continue;
            }
            var conflict = gameState.towers[towerIdx(i, row(newPos))];
            if (conflict != null) {
                if (mob.y < conflict.y + gameState.squareSize)
                    gameState.mobs[i][j].y += gameState.mobs[i][j].speed * 10; // Moved inside tower, now hurry back out
                if (mob.fireCounter > 0) {
                    mob.fireCounter--;
                } else {
                    gameState.mobs[i][j].fire();
                    conflict.hp -= mob.damage;
                    if (conflict.hp <= 0)
                        killTower(conflict.row, conflict.col);
                    mob.fireCounter = mob.rof;
                }
            } else {
                gameState.mobs[i][j].y = newPos;
            }
        }
    }

}
