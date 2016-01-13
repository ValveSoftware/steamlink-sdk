import QtQuick 2.0

Rectangle {
    width: 400; height: 400
    Item {
        scale: .5
        rotation: 15
        transformOrigin: "Center"
        x: 10; y: 10
        Rectangle {
            id: myRect
            objectName: "MyRect"
            x: 5
            width: 100; height: 100
            transformOrigin: "BottomLeft"
            color: "red"
        }
    }
    MouseArea {
        id: clickable
        anchors.fill: parent
    }

    Item {
        x: 200; y: 200
        rotation: 52;
        scale: 2
        Item {
            id: newParent
            x: 100; y: 100
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
