import QtQuick 2.0
Rectangle {
    id: root
    color: "blue"
    width: 100; height: 100
    property bool success: true

    Loader {
        active: false
        anchors.fill: parent
        sourceComponent: Rectangle { color: "red" }
        onLoaded: { root.success = false; } // shouldn't be triggered if active is false
    }
}
