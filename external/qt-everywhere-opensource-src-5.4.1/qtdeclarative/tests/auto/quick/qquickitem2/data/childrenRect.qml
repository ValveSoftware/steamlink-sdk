import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    property int childCount: 0;

    Item {
        objectName: "testItem"
        width: childrenRect.width
        height: childrenRect.height

        Repeater {
            id: repeater
            model: childCount
            delegate: Rectangle {
                x: index*10
                y: index*20
                width: 10
                height: 20

                color: "red"
            }
        }
    }
}
