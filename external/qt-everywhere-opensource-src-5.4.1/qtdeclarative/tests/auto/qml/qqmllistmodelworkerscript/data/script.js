WorkerScript.onMessage = function(msg) {
    var result = null
    try {
        for (var i=0; i<msg.commands.length; i++) {
            var c = 'msg.model.' + msg.commands[i]
            result = eval(c)
        }
        msg.model.sync()
    } catch(e) { }
    WorkerScript.sendMessage({'done': true, 'result': result})
}


