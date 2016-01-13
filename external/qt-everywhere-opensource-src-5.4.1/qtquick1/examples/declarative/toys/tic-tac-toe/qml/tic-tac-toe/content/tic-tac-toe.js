function winner(board)
{
    for (var i=0; i<3; ++i) {
        if (board.children[i].state != ""
                && board.children[i].state == board.children[i+3].state
                && board.children[i].state == board.children[i+6].state)
            return true

        if (board.children[i*3].state != ""
                && board.children[i*3].state == board.children[i*3+1].state
                && board.children[i*3].state == board.children[i*3+2].state)
            return true
    }

    if (board.children[0].state != ""
            && board.children[0].state == board.children[4].state != ""
            && board.children[0].state == board.children[8].state != "")
        return true

    if (board.children[2].state != ""
            && board.children[2].state == board.children[4].state != ""
            && board.children[2].state == board.children[6].state != "")
        return true

    return false
}

function restartGame()
{
    game.running = true

    for (var i=0; i<9; ++i)
        board.children[i].state = ""
}

function makeMove(pos, player)
{
    board.children[pos].state = player
    if (winner(board)) {
        gameFinished(player + " wins")
        return true
    } else {
        return false
    }
}

function canPlayAtPos(pos)
{
    return board.children[pos].state == ""
}

function computerTurn()
{
    var r = Math.random();
    if (r < game.difficulty)
        smartAI();
    else
        randomAI();
}

function smartAI()
{
    function boardCopy(a) {
        var ret = new Object;
        ret.children = new Array(9);
        for (var i = 0; i<9; i++) {
            ret.children[i] = new Object;
            ret.children[i].state = a.children[i].state;
        }
        return ret;
    }

    for (var i=0; i<9; i++) {
        var simpleBoard = boardCopy(board);
        if (canPlayAtPos(i)) {
            simpleBoard.children[i].state = "O";
            if (winner(simpleBoard)) {
                makeMove(i, "O")
                return
            }
        }
    }
    for (var i=0; i<9; i++) {
        var simpleBoard = boardCopy(board);
        if (canPlayAtPos(i)) {
            simpleBoard.children[i].state = "X";
            if (winner(simpleBoard)) {
                makeMove(i, "O")
                return
            }
        }
    }

    function thwart(a,b,c) {    //If they are at a, try b or c
        if (board.children[a].state == "X") {
            if (canPlayAtPos(b)) {
                makeMove(b, "O")
                return true
            } else if (canPlayAtPos(c)) {
                makeMove(c, "O")
                return true
            }
        }
        return false;
    }

    if (thwart(4,0,2)) return;
    if (thwart(0,4,3)) return;
    if (thwart(2,4,1)) return;
    if (thwart(6,4,7)) return;
    if (thwart(8,4,5)) return;
    if (thwart(1,4,2)) return;
    if (thwart(3,4,0)) return;
    if (thwart(5,4,8)) return;
    if (thwart(7,4,6)) return;

    for (var i =0; i<9; i++) {
        if (canPlayAtPos(i)) {
            makeMove(i, "O")
            return
        }
    }
    restartGame();
}

function randomAI()
{
    var unfilledPosns = new Array();

    for (var i=0; i<9; ++i) {
        if (canPlayAtPos(i))
            unfilledPosns.push(i);
    }

    if (unfilledPosns.length == 0) {
        restartGame();
    } else {
        var choice = unfilledPosns[Math.floor(Math.random() * unfilledPosns.length)];
        makeMove(choice, "O");
    }
}

function gameFinished(message)
{
    messageDisplay.text = message
    messageDisplay.visible = true
    game.running = false
}

