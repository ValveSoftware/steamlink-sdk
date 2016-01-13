import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        id: myRect
        objectName: "MyRect"
        width: 50; height: 50
        color: "green";
        anchors.left: parent.left
        anchors.leftMargin: 5
    }
    states: State {
        name: "right"
        AnchorChanges {
            target: myRect;
            anchors.left: undefined
            anchors.right: parent.right
        }
    }
}
