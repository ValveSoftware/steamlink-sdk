import QtQuick 2.0

Item {
    width: 360
    height: 480

    Loader {
        asynchronous: true
        sourceComponent: viewComponent
    }

    Component {
        id: viewComponent
        Column {
            objectName: "container"
            Repeater {
                id: repeater
                objectName: "repeater"

                model: 10

                delegate: Rectangle {
                    objectName: "delegate" + index
                    color: "red"
                    width: 360
                    height: 50
                    Text { text: index }
                }
            }
        }
    }
}
