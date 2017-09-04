import QtQuick 2.0

Item {
    width: 300
    height: 300

    Item {
        anchors.centerIn: parent

        Rectangle {
            objectName: "childrenRectProxy"
            x: container.childrenRect.x
            y: container.childrenRect.y
            width: container.childrenRect.width
            height: container.childrenRect.height
            color: "red"
            opacity: 0.5
        }

        Item {
            id: container

            Rectangle {
                x: -100
                y: -100
                width: 10
                height: 10
                color: "red"
            }

            Rectangle {
                x: -60
                y: -60
                width: 10
                height: 10
                color: "red"
            }
        }

        Rectangle {
            id: origin
            width: 5
            height: 5
            color: "black"
        }
    }
}
