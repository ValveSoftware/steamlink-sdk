import QtQuick 1.0

Rectangle {
    width:360;
    height: 200

    Item {
        objectName: "theItem"
        anchors.centerIn: parent
        width: childrenRect.width
        height: childrenRect.height
        Rectangle {
            id: header1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: 100; height: 50
            color: "green"
        }
        Rectangle {
            id: text1
            anchors.top: header1.bottom
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            width: 100; height: 50
            color: "blue"
        }
    }

    states: [
    State {
        name: "row"
        AnchorChanges {
            target: header1
            anchors.horizontalCenter: undefined
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.top: undefined
        }
        AnchorChanges {
            target: text1
            anchors.horizontalCenter: undefined
            anchors.verticalCenter: parent.verticalCenter
            anchors.top: undefined
            anchors.left: header1.right
        }
        PropertyChanges {
            target: text1
            anchors.leftMargin: 10
            anchors.topMargin: 0
        }
    }
    ]
}
