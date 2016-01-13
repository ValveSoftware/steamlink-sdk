import QtQuick 1.0

Grid {
    id: myGrid

    width: 270
    height: 270
    x: 3
    y: 3
    columns: 4
    spacing: 3

    add: columnTransition
    move: columnTransition

    Repeater {
        model: 20
        Rectangle { color: "black"; width: 50; height: 50 }
    }

    data: [
    Transition {
        id: rowTransition
        objectName: "rowTransition"
        NumberAnimation {
            properties: "x,y";
            easing.type: "OutInCubic"
        }
    },
    Transition {
        id: columnTransition
        objectName: "columnTransition"
        NumberAnimation {
            properties: "x,y";
            easing.type: "OutInCubic"
        }
    }
    ]
}
