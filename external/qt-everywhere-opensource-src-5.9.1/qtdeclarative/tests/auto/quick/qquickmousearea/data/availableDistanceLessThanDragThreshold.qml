import QtQuick 2.0

Rectangle {
    width: 400
    height: 200

    MouseArea {
        id: mouseArea
        objectName: "mouseArea"
        width: 200
        height: 200

        drag.target: mouseArea
        drag.axis: Drag.XAxis
        drag.minimumX: 0
        drag.maximumX: 200
        drag.threshold: 200

        Rectangle {
            anchors.fill: parent
            color: "red"
        }
    }
}
