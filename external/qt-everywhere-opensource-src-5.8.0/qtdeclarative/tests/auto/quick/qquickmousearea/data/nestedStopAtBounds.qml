import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    MouseArea {
        id: outer
        objectName: "outer"
        x: 50
        y: 50
        width: 300
        height: 300

        drag.target: outer
        drag.filterChildren: true

        Rectangle {
            anchors.fill: parent
            color: "yellow"
        }

        MouseArea {
            id: inner
            objectName: "inner"

            x: 0
            y: 0
            width: 200
            height: 200

            drag.target: inner
            drag.minimumX: 0
            drag.maximumX: 100
            drag.minimumY: 0
            drag.maximumY: 100

            Rectangle {
                anchors.fill: parent
                color: "blue"
            }
        }
    }
}
