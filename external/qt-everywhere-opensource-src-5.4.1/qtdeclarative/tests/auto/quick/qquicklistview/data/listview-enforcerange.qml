import QtQuick 2.0

Rectangle {
    width: 240
    height: 320
    color: "#ffffff"
    Component {
        id: myDelegate
        Item {
            id: wrapper
            objectName: "wrapper"
            height: 20
            width: 240
            Text {
                text: index
            }
            Text {
                x: 30
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                x: 120
                id: textNumber
                objectName: "textNumber"
                text: number
            }
            Text {
                x: 200
                text: wrapper.y
            }
        }
    }

    Component {
        id: myHighlight
        Rectangle {
            color: "lightsteelblue"
        }
    }

    ListView {
        id: list
        objectName: "list"
        width: 240
        height: 320
        cacheBuffer: 60
        model: testModel
        delegate: myDelegate
        highlight: myHighlight
        preferredHighlightBegin: 100
        preferredHighlightEnd: 100
        highlightRangeMode: "StrictlyEnforceRange"
    }
}
