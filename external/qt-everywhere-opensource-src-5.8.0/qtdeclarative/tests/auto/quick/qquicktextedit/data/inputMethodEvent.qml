import QtQuick 2.0

TextEdit {
    width: 300
    focus: true

    cursorDelegate: Item {
        objectName: "cursor"
    }
}
