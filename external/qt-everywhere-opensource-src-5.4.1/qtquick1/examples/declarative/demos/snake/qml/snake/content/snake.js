
var snake = new Array;
var board = new Array;
var links = new Array;
var scheduledDirections = new Array;
var numRows = 1;
var numColumns = 1;
var linkComponent = Qt.createComponent("content/Link.qml"); // XXX should resolve relative to script, not component
var cookieComponent = Qt.createComponent("content/Cookie.qml");
var cookie;
var linksToGrow = 0;
var linksToDie = 0;
var waitForCookie = 0;
var growType = 0;
var skullMovementsBeforeDirectionChange = 0;


function rand(n)
{
  return (Math.floor(Math.random() * n));
}

function scheduleDirection(dir)
{
    if (state == "starting") {
        direction = dir;
        headDirection = direction;
        head.rotation = headDirection;
    } else if (state == "running"){
        direction = dir;
        if(scheduledDirections[scheduledDirections.length-1]!=direction)
            scheduledDirections.push(direction);
    }
}

function startNewGame()
{
    if (state == "starting") {
        return;
    }

    if (activeGame) {
        endGame();
        startNewGameTimer.running = true;
        return;
    }

    state = "starting";

    numRows = numRowsAvailable;
    numColumns = numColumnsAvailable;
    board = new Array(numRows * numColumns);
    snake = new Array;
    scheduledDirections = new Array;
    growType = 0;

    skull.z = numRows * numColumns + 1;

    for (var i = 0; i < numRows * numColumns; ++i) {
        if (i < links.length) {
            var link = links[i];
            link.spawned = false;
            link.dying = false;
        } else {
            if(linkComponent.status != Component.Ready) {
                if (linkComponent.status == Component.Error)
                    console.log(linkComponent.errorString());
                else
                    console.log("Still loading linkComponent");
                continue;//TODO: Better error handling?
            }
            var link = linkComponent.createObject(playfield);
            link.z = numRows * numColumns + 1 - i;
            link.type = i == 0 ? 2 : 0;
            link.spawned = false;
            link.dying = false;
            links.push(link);
        }
    }

    head = links[0];
    snake.push(head);
    head.row = numRows/2 -1;
    head.column = numColumns/2 -1;
    head.spawned = true;

    linksToGrow = 5;
    linksToDie = 0;
    waitForCookie = 5;
    score = 0;
    startHeartbeatTimer.running = true;
}

function endGame()
{
    activeGame = false;
    for(var i in snake)
        snake[i].dying = true;
    if (cookie) {
        cookie.dying = true;
        cookie = 0;
    }
    lastScore = score;
    highScores.saveScore(lastScore);
    state = "";
}

function move() {

    if (!head)
        return;

    var dir = direction;

    if (scheduledDirections.length) {
        dir = scheduledDirections.shift();
    }

    if (state == "starting") {
        var turn = (dir - headDirection);
        head.rotation += turn == -3 ? 1 : (turn == 3 ? -1 : turn );
        headDirection = dir;
        return;
    }

    var row = head.row;
    var column = head.column;

    if (dir == 0) {
        row = row - 1;
    } else if (dir == 1) {
        column = column + 1
    } else if (dir == 2) {
        row = row + 1;
    } else if (dir == 3) {
        column = column - 1;
    }

    //validate the new position
    if (row < 0 || row >= numRows
        || column < 0 || column >= numColumns
        || (row == skull.row && column == skull.column)
        || !isFree(row, column)) {
        var turn = (dir - headDirection);
        head.rotation += turn == -3 ? 1 : (turn == 3 ? -1 : turn );
        headDirection = dir;
        endGame();
        return;
    }

    var newLink;
    if (linksToGrow > 0) {
        --linksToGrow;
        newLink = links[snake.length];
        newLink.spawned = false;
        newLink.rotation = snake[snake.length-1].rotation;
        newLink.type = growType;
        newLink.dying = false;
        snake.push(newLink);
    } else {
        var lastLink = snake[snake.length-1];
        board[lastLink.row * numColumns + lastLink.column] = undefined;
    }

    if (waitForCookie > 0) {
        if (--waitForCookie == 0)
            createCookie(cookie? (cookie.value+1) : 1);
    }

    for (var i = snake.length-1; i > 0; --i) {
        snake[i].row = snake[i-1].row;
        snake[i].column = snake[i-1].column;
        snake[i].rotation = snake[i-1].rotation;
    }

    if (newLink) {
        newLink.spawned = true;
    }

    // move the head
    head.row = row;
    head.column = column;
    board[row * numColumns + column] = head;

    var turn = (dir - headDirection);
    head.rotation += turn == -3 ? 1 : (turn == 3 ? -1 : turn );
    headDirection = dir;

    var value = testCookie(row, column);
    if (value > 0) {
        linksToGrow += value;
        score += value;
    }
}

function isFree(row, column)
{
    return board[row * numColumns + column] == undefined;
}

function isHead(row, column)
{
    return head.column == column && head.row == row;
}

function testCookie(row, column)
{
    if (cookie && !cookie.dying && cookie.row == row && cookie.column == column) {
        var value = cookie.value;
        waitForCookie = value;
        growType = snake[snake.length-1].type == 1 ? 0 : 1;
        cookie.dying = true;
        cookie.z = numRows * numColumns + 2;
        return value;
    }
    return 0;
}

function moveSkull()
{

    if (linksToDie > 0) {
        --linksToDie;
        var link = snake.pop();
        link.dying = true;
        board[link.row * numColumns + link.column] = undefined;
        if (score > 0)
            --score;
        if (snake.length == 0) {
            endGame();
            return;
        }
    }

    var row = skull.row;
    var column = skull.column;
    if (isHead(row, column)) {
        endGame();
        return;
    }
    row +=  skull.verticalMovement;
    column += skull.horizontalMovement;

    var attempts = 4;

    while (skullMovementsBeforeDirectionChange == 0 || row < 0 || row >= numRows
        || column < 0 || column >= numColumns
        || (!isFree(row, column) && !isHead(row, column))) {
        var d = rand(8);
        skull.verticalMovement = 0;
        skull.horizontalMovement = 0;
        skullMovementsBeforeDirectionChange = rand(20)+1;
        if (d == 0) {
            skull.verticalMovement = -1
        } else if (d == 1) {
            skull.horizontalMovement = -1;
        } else if (d == 2) {
            skull.verticalMovement = 1
        } else if (d == 3){
            skull.horizontalMovement = 1;
        } else if (cookie) {
            var rd = cookie.row - skull.row;
            var rc = cookie.column - skull.column;
            if (Math.abs(rd) > Math.abs(rc)) {
                skull.verticalMovement = rd > 0 ? 1 : -1;
                skullMovementsBeforeDirectionChange = Math.abs(rd);
            } else {
                skull.horizontalMovement= rc > 0 ? 1 : -1;
                skullMovementsBeforeDirectionChange = Math.abs(rc);
            }
        }
        row = skull.row + skull.verticalMovement;
        column = skull.column + skull.horizontalMovement;
        if (--attempts == 0)
            return;
    }

    skull.row = row;
    skull.column = column;
    --skullMovementsBeforeDirectionChange;
    var value = testCookie(row, column);
    if (value > 0)
        linksToDie += value/2;

    if (isHead(row, column))
        endGame();
}

function createCookie(value) {
    if (numRows * numColumns - snake.length < 10)
        return;

    var column = rand(numColumns);
    var row = rand(numRows);
    while (!isFree(row, column)) {
        column++;
        if (column == numColumns) {
            column = 0;
            row++;
            if (row == numRows)
                row = 0;
        }
    }

    if(cookieComponent.status != Component.Ready) {
        if(cookieComponent.status == Component.Error)
            console.log(cookieComponent.errorString());
        else
            console.log("Still loading cookieComponent");
        return;//TODO: Better error handling?
    }
    cookie = cookieComponent.createObject(head.parent);
    cookie.value = value;
    cookie.row = row;
    cookie.column = column;
}
