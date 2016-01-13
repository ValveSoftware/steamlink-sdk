import QtQuick 1.0

Item {
    id: item
    property variant model
    property bool done: false
    property variant result

    function evalExpressionViaWorker(commands) {
        done = false
        worker.sendMessage({'commands': commands, 'model': model})
    }

    WorkerScript {
        id: worker
        source: "script.js"
        onMessage: {
            item.result = messageObject.result
            item.done = true
        }
    }
}
