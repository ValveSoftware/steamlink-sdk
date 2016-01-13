import QtQuick 2.0

Item {
    id: root
    width: 300; height: 300

    Component {
        id: comp
        Rectangle {
            objectName: "comp"
            parent: root
            anchors.fill: parent
            color: "blue"
        }
    }

    Loader {
        width: 200; height: 200
        sourceComponent: comp
    }
}
