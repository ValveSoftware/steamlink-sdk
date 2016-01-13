import QtQuick 1.0

Rectangle {
    width: 350; height: 220; color: "white"
    Component {
        id: myDelegate
        Item {
            id: wrapper
            width: 180; height: 40;
            opacity: PathView.opacity
            Column {
                x: 5; y: 5
                Text { text: '<b>Name:</b> ' + name }
                Text { text: '<b>Number:</b> ' + number }
            }
        }
    }

    PathView {
        preferredHighlightBegin: 0.1
        preferredHighlightEnd: 0.1
        dragMargin: 5.0
        id: pathView
        objectName: "pathView"
        anchors.fill: parent
        model: listModel
        delegate: myDelegate
        focus: true
        path: Path {
            id: myPath
            objectName: "path"
            startX: 220; startY: 200
            PathAttribute { name: "opacity"; value: 1.0; objectName: "pathAttribute"; }
            PathQuad { x: 220; y: 25; controlX: 260; controlY: 75 }
            PathAttribute { name: "opacity"; value: 0.3 }
            PathQuad { x: 220; y: 200; controlX: -20; controlY: 75 }
        }
        Timer {
            interval: 2000; running: true; repeat: true
            onTriggered: {
                if (pathView.path == alternatePath)
                    pathView.path = myPath;
                else
                    pathView.path = alternatePath;
            }
        }
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
    },
    Path {
        id: alternatePath
        objectName: "alternatePath"
        startX: 100; startY: 40
        PathAttribute { name: "opacity"; value: 0.0 }
        PathLine { x: 100; y: 160 }
        PathAttribute { name: "opacity"; value: 0.2 }
        PathLine { x: 300; y: 160 }
        PathAttribute { name: "opacity"; value: 0.0 }
        PathLine { x: 300; y: 40 }
        PathAttribute { name: "opacity"; value: 0.2 }
        PathLine { x: 100; y: 40 }
    }
    ]
}


