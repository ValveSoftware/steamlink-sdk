import Qt.test 1.0
import QtQuick 2.0

MyRectangle {
    id: rect
    width: 100; height: 100
    color: "red"

    states: State {
        name: "aBlueDay"
        PropertyChanges {
            target: rect
            onPropertyWithNotifyChanged: { rect.color = "blue"; }
        }
    }

    Component.onCompleted: rect.state = "aBlueDay"
}

