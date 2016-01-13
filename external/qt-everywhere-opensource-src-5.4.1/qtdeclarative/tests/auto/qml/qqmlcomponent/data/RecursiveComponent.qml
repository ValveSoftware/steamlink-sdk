import QtQuick 2.0

Item {
    id: inner
    property Item innermost: null

    Component.onCompleted: {
        innermost = Qt.createComponent("./RecursiveComponent.qml").createObject();
    }
}
