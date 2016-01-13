import QtQuick 2.0

Item {
  id: item
  property variant model
  property bool done: false

  WorkerScript {
    id: worker
    source: "workerremovelist.js"
    onMessage: {
      item.done = true
    }
  }

  function addList() {
    model.append({ 'data': [ { 'subData': 1 } ] });

    var element = model.get(0);
  }

  function removeListViaWorker() {
    done = false
    var msg = { 'action': 'removeList', 'model': model }
    worker.sendMessage(msg);
  }

  function doSync() {
    done = false
    var msg = { 'action': 'dosync', 'model': model }
    worker.sendMessage(msg);
  }
}
