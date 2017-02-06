WorkerScript.onMessage = function(msg) {
    if (msg.action == 'removeItem') {
        msg.model.remove(0);
    } else if (msg.action == 'dosync') {
        msg.model.sync();
    }
    WorkerScript.sendMessage({'done': true})
}
