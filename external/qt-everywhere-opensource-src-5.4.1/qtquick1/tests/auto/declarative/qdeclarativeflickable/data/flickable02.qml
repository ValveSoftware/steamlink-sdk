import QtQuick 1.0

Flickable {
    width: 100; height: 100
    contentWidth: row.width; contentHeight: row.height

    Row {
        id: row
        Repeater {
            model: 4
            Rectangle { width: 200; height: 300; color: "blue" }
        }
    }
}
