import QtQml 2.0

QtObject {
    Timer {
        running: true
        interval: 1
        onTriggered: Qt.quit();
    }
}
