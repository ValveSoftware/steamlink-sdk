import QtQuick 2.0

Item {
    id: root

    function testExternalObject() {
        worker.sendMessage(Qt.vector3d(1,2,3));
    }

    WorkerScript {
        id: worker
        source: "script.js"
    }
}
