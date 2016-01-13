import QtQuick 2.0

Column {
    Repeater {
        model: 3
        Rectangle {
            width: 100
            height: 40
            border.width: 2
            color: "red"
        }
    }
}
