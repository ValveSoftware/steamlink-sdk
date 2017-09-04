import QtQuick 2.0

GridView {
    width: 400
    height: 400
    model: 100
    delegate: Rectangle {
        height: 100; width: 100
        color: index % 2 ? "lightsteelblue" : "lightgray"
    }
    contentHeight: contentItem.children.length * 40
}
