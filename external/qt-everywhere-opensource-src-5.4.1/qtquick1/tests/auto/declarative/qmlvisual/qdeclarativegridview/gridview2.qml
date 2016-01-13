import QtQuick 1.0

Rectangle {
    property string skip: "Last bit is wrong (rest is probably right, just bitrot). QTBUG-14838"
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
        ListElement { lColor: "red" }
        ListElement { lColor: "yellow" }
        ListElement { lColor: "green" }
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

    GridView {
        id: gridView; anchors.fill: parent
        cellWidth: 100; cellHeight: 100; cacheBuffer: 200
        model: appModel; delegate: appDelegate; focus: true
        keyNavigationWraps: true

        flickableData: [//Presumably the different way of doing highlight tests more things
            Rectangle {
                color: "transparent"; border.color: "white"; border.width: 8; z: 3000
                height: 100; width: 100
                x: gridView.currentItem.x
                y: gridView.currentItem.y

                Behavior on x { SmoothedAnimation { velocity: 500 } }
                Behavior on y { SmoothedAnimation { velocity: 500 } }
            }
        ]
    }

}
