import QtQuick 1.0

Rectangle {
    id: container
    objectName: "container"
    width: 240
    height: 320
    color: "white"
    Text {
        text: "Zero"
    }
    Repeater {
        id: repeater
        objectName: "repeater"
        width: 240
        height: 320
        model: testData
        Component {
            Text {
                y: index*20
                text: modelData
            }
        }
    }
    Text {
        text: "Last"
    }
}
