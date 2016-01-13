WorkerScript.onMessage = function(msg) {
    world = "World"
    WorkerScript.sendMessage(msg + " " + world)
}

