import QtQuick 1.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        id: myRect
        objectName: "MyRect"
        color: "green";
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
    }
    Item { objectName: "LeftGuideline"; id: leftGuideline; x: 10 }
    Item { objectName: "BottomGuideline"; id: bottomGuideline; y: 150 }
    states: State {
        name: "reanchored"
        AnchorChanges {
            target: myRect;
            anchors.horizontalCenter: bottomGuideline.horizontalCenter
            anchors.verticalCenter: leftGuideline.verticalCenter
        }
    }
}
