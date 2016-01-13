import QtQuick 1.0

Rectangle {
    property variant myInput: input

    width: 800; height: 600; color: "blue"

    TextInput { id: input; focus: true
        readOnly: true
        text: "I am the very model of a modern major general.\n"
    }
}
