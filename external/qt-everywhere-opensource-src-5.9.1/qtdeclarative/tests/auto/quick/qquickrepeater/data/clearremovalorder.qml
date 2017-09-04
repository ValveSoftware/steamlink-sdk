import QtQuick 2.0

Item {
    ListModel {
        id: firstModel
        objectName: "firstModel"

        ListElement {
            name: "Captain"
        }
        ListElement {
            name: "Jack"
        }
        ListElement {
            name: "Sparrow"
        }
    }
    ListModel {
        id: secondModel
        objectName: "secondModel"
    }
    Repeater {
        objectName: "repeater"
        model: firstModel
        delegate: Rectangle {
            width: 5
            height: 5
            color: "green"
        }
    }
}
