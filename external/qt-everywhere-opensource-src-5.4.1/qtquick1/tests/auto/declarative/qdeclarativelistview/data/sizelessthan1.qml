import QtQuick 1.0

Rectangle {
    width: 240
    height: 320
    color: "#ffffff"
    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 0.5
            width: 240
            color: ((index % 2) == 1 ? "red" : "blue")
        }
    }
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
    }
}
