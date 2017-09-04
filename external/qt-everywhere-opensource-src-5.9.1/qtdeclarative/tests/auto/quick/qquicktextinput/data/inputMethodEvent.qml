import QtQuick 2.0

TextInput {
    width: 300
    focus: true
    autoScroll: false

    cursorDelegate: Item { objectName: "cursor" }
}
