import QtQuick 2.0

Item {
    Component.onCompleted: {
        var c = Qt.createComponent("QQmlDataDestroyedComponent.qml")
        var o = c.createObject(null); // JS ownership
    }
}
