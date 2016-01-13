import QtQuick 2.0

MouseArea {
    width: 200; height: 200

    Item {
        id: dragTarget
        objectName: "target"
        x: 50; y: 50
        width: 100; height: 100
    }

    onPressed: {
        drag.target = dragTarget
        drag.axis = Drag.XAxis
    }
    onReleased: drag.target = null
}
