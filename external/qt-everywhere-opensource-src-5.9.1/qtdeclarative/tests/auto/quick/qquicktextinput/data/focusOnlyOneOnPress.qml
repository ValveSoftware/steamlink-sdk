import QtQuick 2.2

Rectangle {
    width: 400
    height: 400

    Column {
        spacing: 5
        TextInput {
            objectName: "first"
            onEditingFinished: second.focus = true
            width: 100
            Rectangle { anchors.fill: parent; color: parent.activeFocus ? "red" : "blue"; opacity: 0.3 }
        }
        TextInput {
            id: second
            objectName: "second"
            onEditingFinished: third.focus = true
            width: 100
            Rectangle { anchors.fill: parent; color: parent.activeFocus ? "red" : "blue"; opacity: 0.3 }
        }
        TextInput {
            objectName: "third"
            id: third
            width: 100
            Rectangle { anchors.fill: parent; color: parent.activeFocus ? "red" : "blue"; opacity: 0.3 }
        }
        Component.onCompleted: {
            second.focus = true
        }
    }
}
