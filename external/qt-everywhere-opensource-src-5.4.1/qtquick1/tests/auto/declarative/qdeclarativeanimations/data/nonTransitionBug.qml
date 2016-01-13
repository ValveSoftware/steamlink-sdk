import QtQuick 1.0

Rectangle {
    id: root
    width: 200
    height: 200

    Rectangle {
        id: mover
        objectName: "mover"
    }

    states: [
        State {
            name: "free"
        },
        State {
            name: "left"
            PropertyChanges {
                restoreEntryValues: false
                target: mover
                x: 0
            }
        }
    ]

    transitions: Transition {
        PropertyAnimation { properties: "x"; duration: 50 }
    }
}
