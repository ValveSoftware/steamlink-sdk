import QtQuick 2.0

Item{
    width: 200
    height: 200
    property int doubleClicks: 0
    property int clicks: 0
    property int pressAndHolds: 0
    property int presses: 0
    MouseArea{
        z: 0
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: presses++
        onClicked: clicks++
        onPressAndHold: pressAndHolds++
        onDoubleClicked: doubleClicks++
    }
    MouseArea{
        z: 1
        propagateComposedEvents: true
        enabled: true
        anchors.fill: parent
    }
}
