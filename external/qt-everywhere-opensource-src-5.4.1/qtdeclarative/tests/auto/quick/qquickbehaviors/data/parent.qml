import QtQuick 2.0
Rectangle {
    width: 400
    height: 400
    Rectangle {
        id: rect
        objectName: "MyRect"
        width: 100; height: 100; color: "green"
        Behavior on parent {
            SequentialAnimation {
                PauseAnimation { duration: 500 }
                PropertyAction {}
            }
        }
    }
    Item {
        id: newParent
        objectName: "NewParent"
        x: 100
    }
    states: State {
        name: "reparented"
        PropertyChanges {
            target: rect
            parent: newParent
        }
    }
}
