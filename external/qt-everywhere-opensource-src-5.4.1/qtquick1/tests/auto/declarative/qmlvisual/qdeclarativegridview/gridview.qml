import QtQuick 1.0

Rectangle {
    width: 300; height: 400; color: "black"

    ListModel {
        id: appModel
        ListElement { lColor: "red" }
        ListElement { lColor: "yellow" }
        ListElement { lColor: "green" }
        ListElement { lColor: "blue" }
        ListElement { lColor: "purple" }
        ListElement { lColor: "orange" }
        ListElement { lColor: "pink" }
        ListElement { lColor: "brown" }
        ListElement { lColor: "gray" }
        ListElement { lColor: "red" }
        ListElement { lColor: "yellow" }
        ListElement { lColor: "green" }
        ListElement { lColor: "blue" }
        ListElement { lColor: "purple" }
        ListElement { lColor: "orange" }
        ListElement { lColor: "pink" }
        ListElement { lColor: "brown" }
        ListElement { lColor: "gray" }
    }

    Component {
        id: appDelegate
        Item {
            width: 100; height: 100
            Rectangle {
                color: lColor; x: 4; y: 4
            width: 92; height: 92
        }
        }
    }

    Component {
        id: appHighlight
        Rectangle { width: 100; height: 100; color: "white"; z: 0 }
    }

    GridView {
        anchors.fill: parent
        cellWidth: 100; cellHeight: 100; cacheBuffer: 200
        model: appModel; delegate: appDelegate
        highlight: appHighlight
        focus: true
    }
}
