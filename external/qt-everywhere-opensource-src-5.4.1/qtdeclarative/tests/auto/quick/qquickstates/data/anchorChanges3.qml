import QtQuick 2.0

Rectangle {
    id: container
    width: 200; height: 200
    Rectangle {
        id: myRect
        objectName: "MyRect"
        color: "green";
        anchors.left: parent.left
        anchors.right: rightGuideline.left
        anchors.top: topGuideline.top
        anchors.bottom: container.bottom
    }
    Item { objectName: "LeftGuideline"; id: leftGuideline; x: 10 }
    Item { id: rightGuideline; x: 150 }
    Item { id: topGuideline; y: 10 }
    Item { objectName: "BottomGuideline"; id: bottomGuideline; y: 150 }
    states: State {
        name: "reanchored"
        AnchorChanges {
            target: myRect;
            anchors.left: leftGuideline.left
            anchors.right: container.right
            anchors.top: container.top
            anchors.bottom: bottomGuideline.bottom
        }
    }
}
