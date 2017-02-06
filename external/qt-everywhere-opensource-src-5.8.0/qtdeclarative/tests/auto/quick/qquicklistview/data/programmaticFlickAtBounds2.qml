import QtQuick 2.0

ListView {
    id: view
    width: 200; height: 400

    snapMode: ListView.SnapToItem

    model: 10
    delegate: Rectangle {
        width: 200; height: 50
        color: index % 2 ? "red" : "green"
    }
}
