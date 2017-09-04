import QtQml 2.0

QtObject {
    property int width: 100
    property int height: 62

    property var timer: Timer {
        running: true
        repeat: true
        interval: 50
        onTriggered: height = (2 * height) % 99;
    }
}

