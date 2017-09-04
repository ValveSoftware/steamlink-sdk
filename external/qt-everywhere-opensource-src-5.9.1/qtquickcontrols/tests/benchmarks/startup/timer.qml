import QtQuick 2.2

Timer {
    running: true
    interval: 0
    onTriggered: Qt.quit();
}
