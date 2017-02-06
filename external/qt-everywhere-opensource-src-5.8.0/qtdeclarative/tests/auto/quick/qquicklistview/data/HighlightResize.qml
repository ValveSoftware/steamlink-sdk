import QtQuick 2.0

ListView {
    id: view

    width: 400
    height: 400
    model: 5

    highlight: Rectangle {
        color: "skyblue"
    }

    delegate: Item {
        width: 100 + index * 20
        height: 100 + index * 10

        Text {
            anchors.centerIn: parent
            text: model.index
        }
    }
}
