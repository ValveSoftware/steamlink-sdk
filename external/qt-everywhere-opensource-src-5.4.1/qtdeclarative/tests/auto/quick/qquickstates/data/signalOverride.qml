import QtQuick 2.0
import Qt.test 1.0

MyRectangle {
    id: rect

    onDidSomething: color = "blue"

    width: 100; height: 100
    color: "red"
    states: State {
        name: "green"
        PropertyChanges {
            target: rect
            onDidSomething: color = "green"
        }
    }
}
