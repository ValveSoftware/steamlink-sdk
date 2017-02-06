WorkerScript.onMessage = function(msg) {
    var res = Qt.include("Global.js");
    WorkerScript.sendMessage(msg + " " + data)
}

