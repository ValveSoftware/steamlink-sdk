import QtQuick 1.0

Item {
    id: rect
    property color color
    property color border : Qt.rgba(0,0,0,0)
    property int rotation
    property int radius
    property int borderWidth
    property bool smooth: false

    width: 40; height: 40
    Item {
        anchors.centerIn: parent; rotation: rect.rotation;
        Rectangle {
            anchors.centerIn: parent; width: 40; height: 40
            color: rect.color; border.color: rect.border; border.width: rect.border != Qt.rgba(0,0,0,0) ? 2 : 0
            radius: rect.radius; smooth: rect.smooth
        }
    }
}
