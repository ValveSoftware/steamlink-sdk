import QtQuick 1.0

GridView {
    anchors.fill: parent
    width: 320; height: 200
    cellWidth: 100; cellHeight: 100; cacheBuffer: 200; focus: true
    keyNavigationWraps: true; highlightFollowsCurrentItem: false

    model: ListModel {
        id: appModel
        ListElement { lColor: "red" }
        ListElement { lColor: "yellow" }
        ListElement { lColor: "green" }
        ListElement { lColor: "blue" }
    }

    delegate: Item {
        width: 100; height: 100
        Rectangle {
            color: lColor; x: 4; y: 4
            width: 92; height: 92
        }
    }

    highlight: Rectangle { width: 100; height: 100; color: "black" }
}
