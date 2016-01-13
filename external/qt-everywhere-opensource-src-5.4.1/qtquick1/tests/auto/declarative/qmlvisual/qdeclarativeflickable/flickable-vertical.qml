import QtQuick 1.0

Rectangle {
    color: "lightSteelBlue"
    width: 200; height: 300

    ListModel {
        id: list
        ListElement { dayColor: "steelblue" }
        ListElement { dayColor: "blue" }
        ListElement { dayColor: "yellow" }
        ListElement { dayColor: "purple" }
        ListElement { dayColor: "red" }
        ListElement { dayColor: "green" }
        ListElement { dayColor: "orange" }
    }

    Flickable {
        id: flick
        height: parent.height-50
        width: parent.width; contentHeight: column.height

        Column {
            id: column
            Repeater {
                model: list
                Rectangle { width: 200; height: 100; color: mr.pressed ? "black" : dayColor
                    MouseArea {
                        id: mr
                        anchors.fill: parent
                    }
                }
            }
        }
        clip: true
    }
    Rectangle {
        radius: 3
        x: flick.width-8
        width: 8
        y: flick.visibleArea.yPosition * flick.height
        height: flick.visibleArea.heightRatio * flick.height
    }

    // click to toggle interactive flag
    Rectangle {
        width: 64
        height: 48
        y: parent.height - 50
        color: "red"
        MouseArea {
            anchors.fill: parent
            onClicked: flick.interactive = flick.interactive ? false : true
        }
    }

    // click to toggle click delay
    Rectangle {
        width: 64
        height: 48
        x: 66
        y: parent.height - 50
        color: "green"
        MouseArea {
            anchors.fill: parent
            onClicked: flick.pressDelay = flick.pressDelay > 0 ? 0 : 500
        }
    }

    // click to toggle overshoot
    Rectangle {
        width: 64
        height: 48
        x: 130
        y: parent.height - 50
        color: "yellow"
        MouseArea {
            anchors.fill: parent
            onClicked: flick.boundsBehavior = flick.boundsBehavior == Flickable.StopAtBounds ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds
        }
    }

    Rectangle {
        width: Math.abs(flick.verticalVelocity)/100
        height: 50
        x: 200
        y: parent.height - 50
        color: "blue"
    }
}
