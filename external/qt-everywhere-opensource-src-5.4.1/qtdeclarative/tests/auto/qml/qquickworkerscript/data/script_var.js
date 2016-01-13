var world = "World"

WorkerScript.onMessage = function(msg) {
    WorkerScript.sendMessage(msg + " " + world)
}

