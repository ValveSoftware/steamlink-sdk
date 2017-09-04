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
            height: 100
            width: 100
            Text {
                text: index
            }
            Text {
                y: 25
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                y: 50
                id: textNumber
                objectName: "textNumber"
                text: number
            }
            Text {
                y: 75
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

    GridView {
        id: grid
        objectName: "grid"
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
        highlight: myHighlight
        flow: (testTopToBottom == true) ? GridView.TopToBottom : GridView.LeftToRight
        layoutDirection: (testRightToLeft == true) ? Qt.RightToLeft : Qt.LeftToRight
        preferredHighlightBegin: 100
        preferredHighlightEnd: 100
        highlightRangeMode: "StrictlyEnforceRange"
        focus: true
    }
}
