import QtQuick 2.0

Item {

    ListModel {
        id: model
        ListElement {
            name: "Bill Smith"
            number: "555 3264"
        }
        ListElement {
            name: "John Brown"
            number: "555 8426"
        }
        ListElement {
            name: "Sam Wise"
            number: "555 0473"
        }
        ListElement {
            name: "Bob Brown"
            number: "555 5845"
        }
    }

    Component {
        id: highlight
        Rectangle {
            objectName: "highlight"
            width: 80; height: 80
            color: "lightsteelblue"; radius: 5
            y: grid.currentItem.y+5
            x: grid.currentItem.x+5
        }
    }

    GridView {
        id: grid
        objectName: "grid"
        anchors.fill: parent
        model: model
        delegate: Text { objectName: "wrapper"; text: name; width: 80; height: 80 }

        highlight: highlight
        highlightFollowsCurrentItem: false
        focus: true
    }

}
