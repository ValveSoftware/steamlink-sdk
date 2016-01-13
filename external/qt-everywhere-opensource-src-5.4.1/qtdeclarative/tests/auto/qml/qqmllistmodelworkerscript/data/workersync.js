WorkerScript.onMessage = function(msg) {
    if (msg.action == 'addItem') {
        msg.model.get(0).level0.append({ 'level1': 33 });
    } else if (msg.action == 'dosync') {
        msg.model.sync();
    }
    WorkerScript.sendMessage({'done': true})
}
