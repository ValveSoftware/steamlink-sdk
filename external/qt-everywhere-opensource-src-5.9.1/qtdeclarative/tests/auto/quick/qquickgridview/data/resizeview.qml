import QtQuick 2.0

Rectangle {
    id: root

    width: 240
    height: 320

    GridView {
        id: grid
        objectName: "grid"
        width: initialWidth
        height: initialHeight
        cellWidth: 80
        cellHeight: 60
        cacheBuffer: 0
        model: testModel
        delegate: Rectangle {
            objectName: "wrapper"
            width: 80
            height: 60
            border.width: 1
        }
    }
}

