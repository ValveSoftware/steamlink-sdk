import QtQuick 1.1

Rectangle {
    id: container
    width: 200; height: 200
    Rectangle {
        id: myRect
        anchors.layoutDirection: Qt.RightToLeft
        objectName: "MyRect"
        color: "green";
        anchors.left: parent.left
        anchors.right: rightGuideline.left
        anchors.top: topGuideline.top
        anchors.bottom: container.bottom
    }
    Item { id: leftGuideline; x: 10 }
    Item { id: rightGuideline; x: 150 }
    Item { id: topGuideline; y: 10 }
    Item { id: bottomGuideline; y: 150 }
    Item { id: topGuideline2; y: 50 }
    Item { id: bottomGuideline2; y: 175 }
    MouseArea {
        id: wholeArea
        anchors.fill: parent
        onClicked: {
            if (container.state == "") {
                container.state = "reanchored";
            } else if (container.state == "reanchored") {
                container.state = "reanchored2";
            } else if (container.state == "reanchored2")
                container.state = "reanchored";
        }
    }

    states: [ State {
        name: "reanchored"
        AnchorChanges {
            target: myRect;
            anchors.left: leftGuideline.left
            anchors.right: container.right
            anchors.top: container.top
            anchors.bottom: bottomGuideline.bottom
        }
    }, State {
        name: "reanchored2"
        AnchorChanges {
            target: myRect;
            anchors.left: undefined
            anchors.right: undefined
            anchors.top: topGuideline2.top
            anchors.bottom: bottomGuideline2.bottom
        }
    }]

    transitions: Transition {
        AnchorAnimation { }
    }

    MouseArea {
        width: 50; height: 50
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onClicked: {
            container.state = "";
        }
    }

    state: "reanchored"
}
