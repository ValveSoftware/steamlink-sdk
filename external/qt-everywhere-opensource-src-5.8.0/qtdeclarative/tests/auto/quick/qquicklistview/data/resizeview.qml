import QtQuick 2.0

Rectangle {
    id: root

    width: 240
    height: 240

    property real initialHeight

    ListView {
        id: list
        objectName: "list"
        width: 240
        cacheBuffer: 0
        height: initialHeight
        model: testModel
        delegate: Rectangle {
            objectName: "wrapper"
            width: 240
            height: 20
            border.width: 1
        }
    }
}

