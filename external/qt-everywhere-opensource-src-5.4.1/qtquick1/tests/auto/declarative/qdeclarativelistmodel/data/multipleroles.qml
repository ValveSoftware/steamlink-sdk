import QtQuick 1.0
ListView {
    width: 100
    height: 250
    delegate: Rectangle {
        width: 100
        height: 50
        color: black ? "black": "white"
    }
    model: ListModel {
        objectName: "listModel"
        ListElement {
            black: false
            rounded: false
        }
        ListElement {
            black: true
            rounded: false
        }
        ListElement {
            black: true
            rounded: false
        }
    }
}
