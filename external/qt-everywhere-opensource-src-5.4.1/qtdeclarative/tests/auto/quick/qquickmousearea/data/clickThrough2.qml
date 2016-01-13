import QtQuick 2.0

Item{
    width: 300
    height: 300
    property int doubleClicks: 0
    property int clicks: 0
    property int pressAndHolds: 0
    property int presses: 0
    property bool letThrough: false
    property bool noPropagation: false
    Rectangle{
        z: 0
        color: "lightsteelblue"
        width: 150
        height: 150
        MouseArea{
            anchors.fill: parent
            propagateComposedEvents: true
            onPressed: presses++
            onClicked: clicks++
            onPressAndHold: pressAndHolds++
            onDoubleClicked: doubleClicks++
        }
    }
    MouseArea{
        z: 1
        enabled: true
        anchors.fill: parent
        propagateComposedEvents: !noPropagation
        onClicked: mouse.accepted = !letThrough;
        onDoubleClicked: mouse.accepted = !letThrough;
        onPressAndHold: mouse.accepted = !letThrough;
    }
}
