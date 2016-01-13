import QtQuick 1.0

Flickable {
    width: 100; height: 200
    contentWidth: column.width; contentHeight: column.height

    Column {
        id: column
        Repeater {
            model: 4
            Rectangle { width: 200; height: 300; color: "blue" }
        }
    }
}
