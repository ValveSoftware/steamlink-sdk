import QtQuick 2.0

Item {
    id: root

    property bool success: false

    Component.onCompleted: {
        for (var i = 0; i < 10; ++i) {
            Qt.createComponent("RecursiveComponent.qml").createObject(root)
        }

        var o = Qt.createComponent("TestComponent.qml").createObject(root)
        root.success = (o != null)
    }
}
