import QtQuick 2.0

Rectangle {
    objectName: "rootItem"
    color: "white"
    width: 800
    height: 600

    TestComponent {
        objectName: "testComponent"

    }

    Rectangle {
        objectName: "rectangleItem"
        gradient: Gradient {

        }
    }

    Item {
        id: item
        objectName: "simpleItem"
        property string testProperty: dynamicProperty
    }

    states: [
        State {
            name: "state01"
            PropertyChanges {
                target: item
                width: 10
            }
        },
        State {
            name: "state02"
        }
    ]
}
