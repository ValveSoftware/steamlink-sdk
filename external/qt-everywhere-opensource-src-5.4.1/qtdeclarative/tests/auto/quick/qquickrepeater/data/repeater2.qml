import QtQuick 2.0

Rectangle {
    width: 240
    height: 320
    color: "white"
    Component {
        id: myDelegate
        Item {
            objectName: "myDelegate"
            height: 20
            width: 240
            Text {
                objectName: "myName"
                text: name
            }
            Text {
                objectName: "myNumber"
                x: 100
                text: number
            }
        }
    }
    Column {
        id: container
        objectName: "container"
        Repeater {
            id: repeater
            objectName: "repeater"
            width: 240
            height: 320
            delegate: myDelegate
            model: testData
        }
    }
}
