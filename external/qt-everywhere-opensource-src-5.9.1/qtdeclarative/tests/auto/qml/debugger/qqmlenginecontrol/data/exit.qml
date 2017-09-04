import QtQuick 2.0

Item {
    Timer {
        running: true
        interval: 1
        onTriggered: Qt.quit();
    }
}
