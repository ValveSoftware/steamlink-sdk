import QtQuick 2.0

ListView {
    width: 400
    height: 400
    model: 100
    delegate: Rectangle {
        height: 40; width: 400
        color: index % 2 ? "lightsteelblue" : "lightgray"
    }
}
