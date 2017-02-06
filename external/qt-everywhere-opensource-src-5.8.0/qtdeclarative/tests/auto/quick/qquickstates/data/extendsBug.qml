import QtQuick 2.0

Rectangle {
    width: 200
    height: 200

    Rectangle {
        id: rect
        objectName: "greenRect"
        width: 100
        height: 100
        color: "green"
    }

    states:[
        State {
            name: "a"
            PropertyChanges { target: rect; x: 100 }
        },
        State {
            name: "b"
            extend:"a"
            PropertyChanges { target: rect; y: 100 }
        }
    ]
}
