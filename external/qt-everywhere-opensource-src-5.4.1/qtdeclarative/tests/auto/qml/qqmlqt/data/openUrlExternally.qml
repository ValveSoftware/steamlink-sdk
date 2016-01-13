import QtQuick 2.0

QtObject {
    Component.onCompleted: Qt.openUrlExternally("test:url")

    property bool testFile
    onTestFileChanged: Qt.openUrlExternally("test.html")
}
