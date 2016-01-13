import QtQuick 1.0

Rectangle {
    width: 400; height: 400
    Rectangle {
        id: myRect
        objectName: "MyRect"
        x: 5; y: 5
        width: 100; height: 100
        color: "red"
    }
    MouseArea {
        id: clickable
        anchors.fill: parent
    }

    Item {
        id: newParent
        transform: Scale { xScale: .5; yScale: .7}
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
