import QtQuick 1.0

QtObject {
    id: root

    property bool test: false
    property alias myalias: root.objectName
    signal go
    onGo: test = true

    Component.onCompleted: {
        root.go();
    }
}
