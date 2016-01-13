import QtQuick 1.0
import Qt.test 1.0

MyRectangle {
    id: rect

    width: 100; height: 100
    states: State {
        name: "overridden"
        PropertyChanges {
            target: rect
            onDidSomething: rect.state = ""
        }
    }
}
