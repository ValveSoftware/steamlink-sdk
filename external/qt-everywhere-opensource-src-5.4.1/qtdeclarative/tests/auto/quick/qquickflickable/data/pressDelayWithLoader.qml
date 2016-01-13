import QtQuick 2.0

Loader {
    id: loader

    width: 300
    height: 300

    sourceComponent: Flickable {
        pressDelay: 1000
        contentWidth: loader.width
        contentHeight: loader.height
        MouseArea {
            anchors.fill: parent
            onPressed: loader.sourceComponent = null
        }
    }
}
