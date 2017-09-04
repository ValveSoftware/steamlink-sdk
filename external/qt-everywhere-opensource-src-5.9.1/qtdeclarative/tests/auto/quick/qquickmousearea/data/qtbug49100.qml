import QtQuick 2.2

ListView {
    id: list
    width: 200
    height: 200
    model: 50
    delegate: Text {
        text: index + 1
        height: 30
        width: parent.width
        MouseArea {
            anchors.fill: parent
        }
        Rectangle {
            anchors.fill: parent
            opacity: 0.5
            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                onReleased: {
                    list.currentIndex = 0;
                    list.positionViewAtIndex(list.currentIndex, ListView.Contain)
                }
            }
        }
    }
    Component.onCompleted: list.positionViewAtIndex(40, ListView.Beginning)
}
