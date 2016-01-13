import QtQuick 2.0
Rectangle {
    id: root
    color: "blue"
    width: 100; height: 100
    property bool success: false

    Loader {
        anchors.fill: parent
        sourceComponent: Rectangle { color: "red" }
        onLoaded: { root.success = true; } // should be triggered since active is true by default
    }
}
