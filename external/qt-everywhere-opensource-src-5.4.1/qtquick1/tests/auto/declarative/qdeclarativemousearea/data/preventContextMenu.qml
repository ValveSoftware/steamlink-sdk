import QtQuick 1.1
import Test 1.0

Item {
    width: 200
    height: 200

    property alias mouseAreaEnabled: mouseArea.enabled
    property alias eventsReceived: receiver.eventCount

    ContextMenuEventReceiver {
        id: receiver
        anchors.fill: parent
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        enabled: true
    }
}
