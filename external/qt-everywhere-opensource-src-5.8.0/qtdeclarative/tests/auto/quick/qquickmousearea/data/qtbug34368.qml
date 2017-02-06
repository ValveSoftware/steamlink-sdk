import QtQuick 2.2

Rectangle {
    id: root
    color: "green"
    width: 200
    height: 200
    property int clicksEnabled: 0
    property int clicksDisabled: 0
    property bool disableLower: false
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        z: 1
        onClicked: {
            mouse.accepted = false;
            clicksEnabled += 1;
            //console.log("Upper click");
        }
    }
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        z: 0
        enabled: !disableLower
        onClicked: {
            mouse.accepted = false;
            clicksDisabled += 1;
            //console.log("Lower click");
        }
    }
    Text {
        anchors.fill: parent
        text: "A: " + clicksEnabled + " B:" + clicksDisabled
        focus: true
        Keys.onSpacePressed: root.disableLower = !root.disableLower;
    }
}
