import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            height: 20
            width: 240
            color: "transparent"
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

    Rectangle {     // current listview item should be always in this area
        y: 100
        height: 20
        width: 240
        color: "purple"
    }

    ListView {
        id: list
        objectName: "list"
        width: 240
        height: 320
        model: testModel
        delegate: myDelegate
        focus: true

        preferredHighlightBegin: 100
        preferredHighlightEnd: 100
        highlightRangeMode: "StrictlyEnforceRange"

        section.property: "number"
        section.delegate: Rectangle { width: 240; height: 10; color: "lightsteelblue" }
    }
}

