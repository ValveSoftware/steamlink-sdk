import QtQuick 1.0
Rectangle {
    id: extendedRect
    objectName: "extendedRect"
    property color extendedColor: "orange"

    width: 100; height: 100
    color: "red"
    states: State {
        name: "green"
        PropertyChanges {
            target: rect
            onDidSomething: {
                extendedRect.color = "green"
                extendedColor = "green"
            }
        }
    }
}
