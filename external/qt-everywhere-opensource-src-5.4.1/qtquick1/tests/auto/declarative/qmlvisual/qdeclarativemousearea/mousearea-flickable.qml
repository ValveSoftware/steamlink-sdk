import QtQuick 1.0

Rectangle {
    width: 400; height: 480
    color: "white"

    Flickable {
        anchors.fill: parent
        contentHeight: 100

        Rectangle {
            id: yellow
            width: 400; height: 120
            color: "yellow"
            MouseArea {
                anchors.fill: parent
                onPressedChanged: pressed ? yellow.color = "lightyellow": yellow.color = "yellow"
            }
        }
        Rectangle {
            id: blue
            width: 400; height: 120
            y: 120
            color: "steelblue"
            MouseArea {
                anchors.fill: parent
                onPressed: blue.color = "lightsteelblue"
                onCanceled: blue.color = "steelblue"
            }
        }
        Rectangle {
            id: red
            y: 240
            width: 400; height: 120
            color: "red"
            MouseArea {
                anchors.fill: parent
                onEntered:  { red.color = "darkred"; tooltip.opacity = 1 }
                onCanceled: { red.color = "red"; tooltip.opacity = 0 }
            }
            Rectangle {
                id: tooltip
                x: 10; y: 20
                width: 100; height: 50
                color: "green"
                opacity: 0
            }
        }

    }

}
