import QtQuick 2.0

Flickable {
    width: 200; height: 200
    contentWidth: row.width; contentHeight: row.height

    topMargin: 20
    bottomMargin: 30
    leftMargin: 40
    rightMargin: 50

    Row {
        id: row
        Repeater {
            model: 4
            Rectangle { width: 400; height: 600; color: "blue" }
        }
    }
}
