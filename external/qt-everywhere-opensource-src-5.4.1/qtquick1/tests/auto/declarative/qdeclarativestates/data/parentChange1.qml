import QtQuick 1.0

Rectangle {
    width: 400; height: 400
    Item {
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

    Item {
        x: -100; y: -50
        Item {
            id: newParent
            objectName: "NewParent"
            x: 248; y: 360
        }
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
