var lastx = 0;
var lasty = 0;

WorkerScript.onMessage = function(message) {
    var ydiff = message.y - lasty;
    var xdiff = message.x - lastx;

    var total = Math.sqrt(ydiff * ydiff + xdiff * xdiff);

    lastx = message.x;
    lasty = message.y;

    WorkerScript.sendMessage( {xmove: xdiff, ymove: ydiff, move: total} );
}

