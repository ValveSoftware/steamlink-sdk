import QtQuick 1.0

QtObject {
    Component.onCompleted: Qt.openUrlExternally("test:url")

    property bool testFile
    onTestFileChanged: Qt.openUrlExternally("test.html")
}
