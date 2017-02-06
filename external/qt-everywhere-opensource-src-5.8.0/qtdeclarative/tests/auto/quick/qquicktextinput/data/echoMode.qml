import QtQuick 2.0

Rectangle {
    property QtObject myInput: input

    width: 400; height: 200; color: "green"

    TextInput { id: input; focus: true
        width: 400; height: 200
        text: "ABCDefgh"
        cursorDelegate: Rectangle {
            objectName: "cursor"
        }
    }
}
