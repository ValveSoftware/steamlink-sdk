import QtQuick 2.0

Item {
    height: 200
    width: 200
    TextEdit {
        objectName: "textedit"
        text: "Hello\nWorld!"
        selectByMouse: true
        cursorDelegate: Rectangle {
            width: 10
            color: "transparent"
            border.color: "red"
        }
    }
}
