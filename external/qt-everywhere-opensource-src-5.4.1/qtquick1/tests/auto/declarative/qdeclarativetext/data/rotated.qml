import QtQuick 1.0

Rectangle {
    width : 200
    height : 100

    Text {
        objectName: "text"
        x: 20
        y: 20
        height : 20
        width : 80
        text : "Something"
        rotation : 30
        transformOrigin : Item.TopLeft
    }
}

