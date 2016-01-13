import QtQuick 1.0

Item {
    id: document
    width: 200; height: 200

    ListView {
        id: serviceListView
        anchors.fill: parent
        model: 100

        preferredHighlightBegin: 90
        preferredHighlightEnd: 90

        highlightRangeMode: ListView.StrictlyEnforceRange

        delegate: Component {
            Item {
                height: 15 + ((serviceListView.currentIndex == index) ? 20 : 0)
                width: 200
                Rectangle { width: 180; height: parent.height - 4; x: 10; y: 2; color: "red" }
            }
        }
    }

    Rectangle {
        y: 90; width: 200; height: 35
        border.color: "black"
        color: "transparent"
    }
}
