import QtQuick 2.0

Item {
    width: 200; height: 200
    Page {
        Rectangle {
            anchors.fill: parent
            color: "lightsteelblue"
        }
        ListView {
            objectName: "listview"
            topMargin: 40
            bottomMargin: 20
            leftMargin: 40
            rightMargin: 20
            highlightMoveVelocity: 100000
            anchors.fill: parent

            model: 20
            delegate: Rectangle {
                color: "skyblue"
                width: 60; height: 60
                Text {
                    id: txt
                    text: "test" + index
                }
            }
        }
    }
}
