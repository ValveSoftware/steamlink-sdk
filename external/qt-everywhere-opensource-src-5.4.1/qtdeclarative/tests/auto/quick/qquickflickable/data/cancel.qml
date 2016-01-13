import QtQuick 2.0

Flickable {
    width: 200; height: 200
    contentWidth: row.width; contentHeight: row.height

    Row {
        id: row
        objectName: "row"
        Repeater {
            model: 4
            Rectangle { width: 400; height: 600; color: "yellow"; border.width: 1 }
        }
    }
}
