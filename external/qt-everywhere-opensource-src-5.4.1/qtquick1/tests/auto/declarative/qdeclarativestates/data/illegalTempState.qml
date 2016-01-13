import QtQuick 1.0

Rectangle {
    id: card
    width: 100; height: 100

    states: [
        State {
            name: "placed"
            PropertyChanges { target: card; state: "idle" }
        },
        State {
            name: "idle"
        }
    ]

    MouseArea {
        anchors.fill: parent
        onClicked: card.state = "placed"
    }
}
