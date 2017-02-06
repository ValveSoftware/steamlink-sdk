import QtQuick 2.0

Rectangle {
    width: 200; height: 200
    Rectangle {
        id: myRect
        objectName: "MyRect"
        color: "green";
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.baseline: parent.baseline
    }
    Item { objectName: "LeftGuideline"; id: leftGuideline; x: 10 }
    Item { objectName: "BottomGuideline"; id: bottomGuideline; y: 150 }
    states: State {
        name: "reanchored"
        AnchorChanges {
            target: myRect;
            anchors.horizontalCenter: bottomGuideline.horizontalCenter
            anchors.baseline: leftGuideline.baseline
        }
    }
}
