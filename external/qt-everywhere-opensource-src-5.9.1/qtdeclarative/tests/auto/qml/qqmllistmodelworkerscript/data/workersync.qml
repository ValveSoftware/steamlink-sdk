import QtQuick 2.0

Item {
  id: item
  property variant model
  property bool done: false

  WorkerScript {
    id: worker
    source: "workersync.js"
    onMessage: {
      item.done = true
    }
  }

  function addItem0() {
    model.append({ 'level0': [ { 'level1': 29 } ] });
    model.append({ 'level0': [ { 'level1': 37 } ] });
  }

  function addItemViaWorker() {
    done = false
    var msg = { 'action': 'addItem', 'model': model }
    worker.sendMessage(msg);
  }

  function doSync() {
    done = false
    var msg = { 'action': 'dosync', 'model': model }
    worker.sendMessage(msg);
  }
}
