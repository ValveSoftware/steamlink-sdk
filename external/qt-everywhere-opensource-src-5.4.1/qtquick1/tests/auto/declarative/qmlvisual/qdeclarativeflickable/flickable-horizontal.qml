import QtQuick 1.0

Rectangle {
    color: "lightSteelBlue"
    width: 600; height: 300

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
        id: flickable
        anchors.fill: parent; contentWidth: row.width

        Row {
            id: row
            Repeater {
                model: list
                Rectangle { width: 200; height: 300; color: dayColor }
            }
        }
    }
    Rectangle {
        radius: 3
        y: flickable.height-8
        height: 8
        x: flickable.visibleArea.xPosition * flickable.width
        width: flickable.visibleArea.widthRatio * flickable.width
    }
}
