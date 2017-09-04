import QtQuick 2.0

Item {
    width: 360
    height: 480

    Repeater {
        id: repeater
        objectName: "repeater"
        anchors.fill: parent
        property var componentCount: 0
        Component {
            Text {
                y: index*20
                text: index
                Component.onCompleted: repeater.componentCount++;
                Component.onDestruction: repeater.componentCount--;
            }
        }
    }

}
