import QtQuick 2.0

Item {
  id: item
  property variant model
  property bool done: false

  WorkerScript {
    id: worker
    source: "workerremoveelement.js"
    onMessage: {
      item.done = true
    }
  }

  function addItem() {
    model.append({ 'data': 1 });

    var element = model.get(0);
  }

  function removeItemViaWorker() {
    done = false
    var msg = { 'action': 'removeItem', 'model': model }
    worker.sendMessage(msg);
  }

  function doSync() {
    done = false
    var msg = { 'action': 'dosync', 'model': model }
    worker.sendMessage(msg);
  }
}
