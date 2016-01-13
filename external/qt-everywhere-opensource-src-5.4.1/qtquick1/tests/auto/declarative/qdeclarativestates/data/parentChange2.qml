import QtQuick 1.0

Rectangle {
    id: newParent
    width: 400; height: 400
    Item {
        scale: .5
        rotation: 15
        x: 10; y: 10
        Rectangle {
            id: myRect
            objectName: "MyRect"
            x: 5
            width: 100; height: 100
            color: "red"
        }
    }
    MouseArea {
        id: clickable
        anchors.fill: parent
    }

    states: State {
        name: "reparented"
        when: clickable.pressed
        ParentChange {
            target: myRect
            parent: newParent
        }
    }
}
