import QtQuick 1.0

WorkerScript {
    id: worker

    property variant response

    signal done()

    function testSend(value) {
        worker.sendMessage(value)
    }

    function compareLiteralResponse(expected) {
        var e = eval('(' + expected + ')')
        return JSON.stringify(worker.response) == JSON.stringify(e)
    }

    onMessage: {
        worker.response = messageObject
        worker.done()
    }
}

