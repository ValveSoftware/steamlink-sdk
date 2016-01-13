import QtQuick 2.0

Rectangle {
    width: 360; height: 120; color: "white"
    Component {
        id: delegate
        Item {
            id: wrapper
            width: 180; height: 40;
            Column {
                x: 5; y: 5
                Text { text: '<b>Name:</b> ' + name }
                Text { text: '<b>Number:</b> ' + number }
            }
        }
    }
    Component {
        id: highlightRed
        Rectangle {
            color: "red"
            radius: 10
            opacity: 0.5
        }
    }
    GridView {
        cellWidth:180
        cellHeight:40
        objectName: "gridView"
        anchors.fill: parent
        model: listModel
        delegate: delegate
        highlight: highlightRed
        focus: true
        keyNavigationWraps: true
        cacheBuffer: 10
        flow: GridView.LeftToRight
    }

    data:[
        ListModel {
            id: listModel
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
        },
        ListModel {
            objectName: "alternateModel"
            ListElement {
                name: "Jack"
                number: "555 8426"
            }
            ListElement {
                name: "Mary"
                number: "555 3264"
            }
        }
    ]
}


