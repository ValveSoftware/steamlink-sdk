import QtQuick 2.0

Item {
    id: test
    property bool testConditionsMet: false
    Component.onCompleted: {
        var c = Qt.createComponent("QQmlDataDestroyedComponent2Derived.qml")
        c.createObject(test); // Cpp ownership, but it will be a RootObjectInCreation until finished beginCreate.
    }
}
