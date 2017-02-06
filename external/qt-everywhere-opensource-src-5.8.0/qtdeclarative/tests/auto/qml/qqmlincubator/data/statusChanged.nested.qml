import QtQuick 2.0

QtObject {
    id: root

    property bool test: false

    Component.onCompleted: {
        var c = Qt.createComponent("statusChanged.qml");
        c.incubateObject(root, null, Qt.Synchronous);
    }
}
