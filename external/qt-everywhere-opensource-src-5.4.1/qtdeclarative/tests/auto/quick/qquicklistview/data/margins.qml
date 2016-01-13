import QtQuick 2.0

Rectangle {
    id: root
    width: 240
    height: 320
    color: "#ffffff"

    Component {
        id: myDelegate
        Rectangle {
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
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }
    ListView {
        id: list
        objectName: "list"
        anchors.fill: parent
        topMargin: 30
        bottomMargin: 50
        highlightMoveVelocity: 100000
        cacheBuffer: 60
        model: testModel
        delegate: myDelegate
    }
}
