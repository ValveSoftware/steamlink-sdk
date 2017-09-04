import QtQuick 2.1
import QtQuick.Window 2.1

Window {
    width: 100
    height: 100
    property alias mouseArea: mouseArea
    property alias textItem: textItem
    MouseArea {
        id: mouseArea
        hoverEnabled: true
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        property bool wasHovered: false
        onPositionChanged: wasHovered = true
        Text {
            id: textItem
            text: "plain text"
            anchors.fill: parent
        }
    }
}
