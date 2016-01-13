import QtQml 2.0

QtObject {
    id: root
    property Timer t: Timer { interval: 1; running: true; onTriggered: Qt.quit(); }
    property Connections c: Connections {
        target: Qt.application
        onAboutToQuit: console.log("End");
    }
    Component.onCompleted: console.log("Start: " + Qt.application.arguments[1]);
}
