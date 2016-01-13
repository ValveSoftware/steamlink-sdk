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

/* This script file handles the game logic */
.pragma library
.import QtQuick.LocalStorage 2.0 as Sql

var maxColumn = 10;
var maxRow = 13;
var types = 3;
var maxIndex = maxColumn*maxRow;
var board = new Array(maxIndex);
var blockSrc = "Block.qml";
var gameDuration;
var component = Qt.createComponent(blockSrc);
var gameCanvas;
var betweenTurns = false;

var puzzleLevel = null;
var puzzlePath = "";

var gameMode = "arcade"; //Set in new game, then tweaks behaviour of other functions
var gameOver = false;

function changeBlock(src)
{
    blockSrc = src;
    component = Qt.createComponent(blockSrc);
}

// Index function used instead of a 2D array
function index(column, row)
{
    return column + row * maxColumn;
}

function timeStr(msecs)
{
    var secs = Math.floor(msecs/1000);
    var m = Math.floor(secs/60);
    var ret = "" + m + "m " + (secs%60) + "s";
    return ret;
}

function cleanUp()
{
    if (gameCanvas == undefined)
        return;
    // Delete blocks from previous game
    for (var i = 0; i < maxIndex; i++) {
        if (board[i] != null)
            board[i].destroy();
        board[i] = null;
    }
    if (puzzleLevel != null){
        puzzleLevel.destroy();
        puzzleLevel = null;
    }
    gameCanvas.mode = ""
}

function startNewGame(gc, mode, map)
{
    gameCanvas = gc;
    if (mode == undefined)
        gameMode = "arcade";
    else
        gameMode = mode;
    gameOver = false;

    cleanUp();

    gc.gameOver = false;
    gc.mode = gameMode;
    // Calculate board size
    maxColumn = Math.floor(gameCanvas.width/gameCanvas.blockSize);
    maxRow = Math.floor(gameCanvas.height/gameCanvas.blockSize);
    maxIndex = maxRow * maxColumn;
    if (gameMode == "arcade") //Needs to be after board sizing
        getHighScore();


    // Initialize Board
    board = new Array(maxIndex);
    gameCanvas.score = 0;
    gameCanvas.score2 = 0;
    gameCanvas.moves = 0;
    gameCanvas.curTurn = 1;
    if (gameMode == "puzzle")
        loadMap(map);
    else//Note that we load them in reverse order for correct visual stacking
        for (var column = maxColumn - 1; column >= 0; column--)
            for (var row = maxRow - 1; row >= 0; row--)
                createBlock(column, row);
    if (gameMode == "puzzle")
        getLevelHistory();//Needs to be after map load
    gameDuration = new Date();
}

var fillFound;  // Set after a floodFill call to the number of blocks found
var floodBoard; // Set to 1 if the floodFill reaches off that node

// NOTE: Be careful with vars named x,y, as the calling object's x,y are still in scope
function handleClick(x,y)
{
    if (betweenTurns || gameOver || gameCanvas == undefined)
        return;
    var column = Math.floor(x/gameCanvas.blockSize);
    var row = Math.floor(y/gameCanvas.blockSize);
    if (column >= maxColumn || column < 0 || row >= maxRow || row < 0)
        return;
    if (board[index(column, row)] == null)
        return;
    // If it's a valid block, remove it and all connected (does nothing if it's not connected)
    floodFill(column,row, -1);
    if (fillFound <= 0)
        return;
    if (gameMode == "multiplayer" && gameCanvas.curTurn == 2)
        gameCanvas.score2 += (fillFound - 1) * (fillFound - 1);
    else
        gameCanvas.score += (fillFound - 1) * (fillFound - 1);
    if (gameMode == "multiplayer" && gameCanvas.curTurn == 2)
        shuffleUp();
    else
        shuffleDown();
    gameCanvas.moves += 1;
    if (gameMode == "endless")
        refill();
    else if (gameMode != "multiplayer")
        victoryCheck();
    if (gameMode == "multiplayer" && !gc.gameOver){
        betweenTurns = true;
        gameCanvas.swapPlayers();//signal, animate and call turnChange() when ready
    }
}

function floodFill(column,row,type)
{
    if (board[index(column, row)] == null)
        return;
    var first = false;
    if (type == -1) {
        first = true;
        type = board[index(column,row)].type;

        // Flood fill initialization
        fillFound = 0;
        floodBoard = new Array(maxIndex);
    }
    if (column >= maxColumn || column < 0 || row >= maxRow || row < 0)
        return;
    if (floodBoard[index(column, row)] == 1 || (!first && type != board[index(column, row)].type))
        return;
    floodBoard[index(column, row)] = 1;
    floodFill(column + 1, row, type);
    floodFill(column - 1, row, type);
    floodFill(column, row + 1, type);
    floodFill(column, row - 1, type);
    if (first == true && fillFound == 0)
        return; // Can't remove single blocks
    board[index(column, row)].dying = true;
    board[index(column, row)] = null;
    fillFound += 1;
}

function shuffleDown()
{
    // Fall down
    for (var column = 0; column < maxColumn; column++) {
        var fallDist = 0;
        for (var row = maxRow - 1; row >= 0; row--) {
            if (board[index(column,row)] == null) {
                fallDist += 1;
            } else {
                if (fallDist > 0) {
                    var obj = board[index(column, row)];
                    obj.y = (row + fallDist) * gameCanvas.blockSize;
                    board[index(column, row + fallDist)] = obj;
                    board[index(column, row)] = null;
                }
            }
        }
    }
    // Fall to the left
    fallDist = 0;
    for (column = 0; column < maxColumn; column++) {
        if (board[index(column, maxRow - 1)] == null) {
            fallDist += 1;
        } else {
            if (fallDist > 0) {
                for (row = 0; row < maxRow; row++) {
                    obj = board[index(column, row)];
                    if (obj == null)
                        continue;
                    obj.x = (column - fallDist) * gameCanvas.blockSize;
                    board[index(column - fallDist,row)] = obj;
                    board[index(column, row)] = null;
                }
            }
        }
    }
}


function shuffleUp()
{
    // Fall up
    for (var column = 0; column < maxColumn; column++) {
        var fallDist = 0;
        for (var row = 0; row < maxRow; row++) {
            if (board[index(column,row)] == null) {
                fallDist += 1;
            } else {
                if (fallDist > 0) {
                    var obj = board[index(column, row)];
                    obj.y = (row - fallDist) * gameCanvas.blockSize;
                    board[index(column, row - fallDist)] = obj;
                    board[index(column, row)] = null;
                }
            }
        }
    }
    // Fall to the left (or should it be right, so as to be left for P2?)
    fallDist = 0;
    for (column = 0; column < maxColumn; column++) {
        if (board[index(column, 0)] == null) {
            fallDist += 1;
        } else {
            if (fallDist > 0) {
                for (row = 0; row < maxRow; row++) {
                    obj = board[index(column, row)];
                    if (obj == null)
                        continue;
                    obj.x = (column - fallDist) * gameCanvas.blockSize;
                    board[index(column - fallDist,row)] = obj;
                    board[index(column, row)] = null;
                }
            }
        }
    }
}

function turnChange()//called by ui outside
{
    betweenTurns = false;
    if (gameCanvas.curTurn == 1){
        shuffleUp();
        gameCanvas.curTurn = 2;
        victoryCheck();
    }else{
        shuffleDown();
        gameCanvas.curTurn = 1;
        victoryCheck();
    }
}

function refill()
{
    for (var column = 0; column < maxColumn; column++) {
        for (var row = 0; row < maxRow; row++) {
            if (board[index(column, row)] == null)
                createBlock(column, row);
        }
    }
}

function victoryCheck()
{
    // Awards bonuses for no blocks left
    var deservesBonus = true;
    if (board[index(0,maxRow - 1)] != null || board[index(0,0)] != null)
        deservesBonus = false;
    // Checks for game over
    if (deservesBonus){
        if (gameCanvas.curTurn = 1)
            gameCanvas.score += 1000;
        else
            gameCanvas.score2 += 1000;
    }
    gameOver = deservesBonus;
    if (gameCanvas.curTurn == 1){
        if (!(floodMoveCheck(0, maxRow - 1, -1)))
            gameOver = true;
    }else{
        if (!(floodMoveCheck(0, 0, -1, true)))
            gameOver = true;
    }
    if (gameMode == "puzzle"){
        puzzleVictoryCheck(deservesBonus);//Takes it from here
        return;
    }
    if (gameOver) {
        var winnerScore = Math.max(gameCanvas.score, gameCanvas.score2);
        if (gameMode == "multiplayer"){
            gameCanvas.score = winnerScore;
            saveHighScore(gameCanvas.score2);
        }
        saveHighScore(gameCanvas.score);
        gameDuration = new Date() - gameDuration;
        gameCanvas.gameOver = true;
    }
}

// Only floods up and right, to see if it can find adjacent same-typed blocks
function floodMoveCheck(column, row, type, goDownInstead)
{
    if (column >= maxColumn || column < 0 || row >= maxRow || row < 0)
        return false;
    if (board[index(column, row)] == null)
        return false;
    var myType = board[index(column, row)].type;
    if (type == myType)
        return true;
    if (goDownInstead)
        return floodMoveCheck(column + 1, row, myType, goDownInstead) ||
               floodMoveCheck(column, row + 1, myType, goDownInstead);
    else
        return floodMoveCheck(column + 1, row, myType) ||
               floodMoveCheck(column, row - 1, myType);
}

function createBlock(column,row,type)
{
    // Note that we don't wait for the component to become ready. This will
    // only work if the block QML is a local file. Otherwise the component will
    // not be ready immediately. There is a statusChanged signal on the
    // component you could use if you want to wait to load remote files.
    if (component.status == 1){
        if (type == undefined)
            type = Math.floor(Math.random() * types);
        if (type < 0 || type > 4) {
            console.log("Invalid type requested");//TODO: Is this triggered by custom levels much?
            return;
        }
        var dynamicObject = component.createObject(gameCanvas,
                {"type": type,
                "x": column*gameCanvas.blockSize,
                "y": -1*gameCanvas.blockSize,
                "width": gameCanvas.blockSize,
                "height": gameCanvas.blockSize,
                "particleSystem": gameCanvas.ps});
        if (dynamicObject == null){
            console.log("error creating block");
            console.log(component.errorString());
            return false;
        }
        dynamicObject.y = row*gameCanvas.blockSize;
        dynamicObject.spawned = true;

        board[index(column,row)] = dynamicObject;
    }else{
        console.log("error loading block component");
        console.log(component.errorString());
        return false;
    }
    return true;
}

function showPuzzleError(str)
{
    //TODO: Nice user visible UI?
    console.log(str);
}

function loadMap(map)
{
    puzzlePath = map;
    var levelComp = Qt.createComponent(puzzlePath);
    if (levelComp.status != 1){
        console.log("Error loading level");
        showPuzzleError(levelComp.errorString());
        return;
    }
    puzzleLevel = levelComp.createObject();
    if (puzzleLevel == null || !puzzleLevel.startingGrid instanceof Array) {
        showPuzzleError("Bugger!");
        return;
    }
    gameCanvas.showPuzzleGoal(puzzleLevel.goalText);
    //showPuzzleGoal should call finishLoadingMap as the next thing it does, before handling more events
}

function finishLoadingMap()
{
    for (var i in puzzleLevel.startingGrid)
        if (! (puzzleLevel.startingGrid[i] >= 0 && puzzleLevel.startingGrid[i] <= 9) )
            puzzleLevel.startingGrid[i] = 0;
    //TODO: Don't allow loading larger levels, leads to cheating
    while (puzzleLevel.startingGrid.length > maxIndex) puzzleLevel.startingGrid.shift();
    while (puzzleLevel.startingGrid.length < maxIndex) puzzleLevel.startingGrid.unshift(0);
    for (var i in puzzleLevel.startingGrid)
        if (puzzleLevel.startingGrid[i] > 0)
            createBlock(i % maxColumn, Math.floor(i / maxColumn), puzzleLevel.startingGrid[i] - 1);

    //### Experimental feature - allow levels to contain arbitrary QML scenes as well!
    //while (puzzleLevel.children.length)
    //    puzzleLevel.children[0].parent = gameCanvas;
    gameDuration = new Date(); //Don't start until we finish loading
}

function puzzleVictoryCheck(clearedAll)//gameOver has also been set if no more moves
{
    var won = true;
    var soFar = new Date() - gameDuration;
    if (puzzleLevel.scoreTarget != -1 && gameCanvas.score < puzzleLevel.scoreTarget){
        won = false;
    } if (puzzleLevel.scoreTarget != -1 && gameCanvas.score >= puzzleLevel.scoreTarget && !puzzleLevel.mustClear){
        gameOver = true;
    } if (puzzleLevel.timeTarget != -1 && soFar/1000.0 > puzzleLevel.timeTarget){
        gameOver = true;
    } if (puzzleLevel.moveTarget != -1 && gameCanvas.moves >= puzzleLevel.moveTarget){
        gameOver = true;
    } if (puzzleLevel.mustClear && gameOver && !clearedAll) {
        won = false;
    }

    if (gameOver) {
        gameCanvas.gameOver = true;
        gameCanvas.showPuzzleEnd(won);

        if (won) {
            // Store progress
            saveLevelHistory();
        }
    }
}

function getHighScore()
{
    var db = Sql.LocalStorage.openDatabaseSync(
        "SameGame",
        "2.0",
        "SameGame Local Data",
        100
    );
    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Scores(game TEXT, score NUMBER, gridSize TEXT, time NUMBER)');
            // Only show results for the current grid size
            var rs = tx.executeSql('SELECT * FROM Scores WHERE gridSize = "'
                + maxColumn + "x" + maxRow + '" AND game = "' + gameMode + '" ORDER BY score desc');
            if (rs.rows.length > 0)
                gameCanvas.highScore = rs.rows.item(0).score;
            else
                gameCanvas.highScore = 0;
        }
    );
}

function saveHighScore(score)
{
    // Offline storage
    var db = Sql.LocalStorage.openDatabaseSync(
        "SameGame",
        "2.0",
        "SameGame Local Data",
        100
    );
    var dataStr = "INSERT INTO Scores VALUES(?, ?, ?, ?)";
    var data = [
        gameMode,
        score,
        maxColumn + "x" + maxRow,
        Math.floor(gameDuration / 1000)
    ];
    if (score >= gameCanvas.highScore)//Update UI field
        gameCanvas.highScore = score;

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Scores(game TEXT, score NUMBER, gridSize TEXT, time NUMBER)');
            tx.executeSql(dataStr, data);
        }
    );
}

function getLevelHistory()
{
    var db = Sql.LocalStorage.openDatabaseSync(
        "SameGame",
        "2.0",
        "SameGame Local Data",
        100
    );
    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Puzzle(level TEXT, score NUMBER, moves NUMBER, time NUMBER)');
            var rs = tx.executeSql('SELECT * FROM Puzzle WHERE level = "' + puzzlePath + '" ORDER BY score desc');
            if (rs.rows.length > 0) {
                gameCanvas.puzzleWon = true;
                gameCanvas.highScore = rs.rows.item(0).score;
            } else {
                gameCanvas.puzzleWon = false;
                gameCanvas.highScore = 0;
            }
        }
    );
}

function saveLevelHistory()
{
    var db = Sql.LocalStorage.openDatabaseSync(
        "SameGame",
        "2.0",
        "SameGame Local Data",
        100
    );
    var dataStr = "INSERT INTO Puzzle VALUES(?, ?, ?, ?)";
    var data = [
        puzzlePath,
        gameCanvas.score,
        gameCanvas.moves,
        Math.floor(gameDuration / 1000)
    ];
    gameCanvas.puzzleWon = true;

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Puzzle(level TEXT, score NUMBER, moves NUMBER, time NUMBER)');
            tx.executeSql(dataStr, data);
        }
    );
}

function nuke() //For "Debug mode"
{
    for (var row = 1; row <= 5; row++) {
        for (var col = 0; col < 5; col++) {
            if (board[index(col, maxRow - row)] != null) {
                board[index(col, maxRow - row)].dying = true;
                board[index(col, maxRow - row)] = null;
            }
        }
    }
    if (gameMode == "multiplayer" && gameCanvas.curTurn == 2)
        shuffleUp();
    else
        shuffleDown();
    if (gameMode == "endless")
        refill();
    else
        victoryCheck();
}
