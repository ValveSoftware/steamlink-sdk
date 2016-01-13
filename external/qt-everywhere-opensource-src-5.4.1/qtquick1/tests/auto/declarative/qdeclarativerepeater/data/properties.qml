import QtQuick 1.0

Row {
    Repeater {
        objectName: "repeater"
        model: 5
        Text {
            text: "I'm item " + index
        }
    }
}
