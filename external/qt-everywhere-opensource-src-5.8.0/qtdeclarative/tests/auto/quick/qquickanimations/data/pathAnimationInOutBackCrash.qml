import QtQuick 2.0
Item {
    id: root
    width: 450; height: 600

    Rectangle {
        objectName:"rect"
        id: rect
        x:200
        y:500
        width: 225; height: 40
        color: "lightsteelblue"
    }
    PathAnimation {
        id:anim
        running:true
        duration: 200

        easing.type: Easing.InOutBack

        target:rect
        path: Path {
            PathLine { x: 0; y: 0 }
        }
    }
}