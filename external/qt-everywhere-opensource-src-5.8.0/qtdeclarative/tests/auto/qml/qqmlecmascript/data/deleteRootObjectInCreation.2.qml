import QtQuick 2.0

Item {
    id: test
    property bool testConditionsMet: false
    Component.onCompleted: {
        var c = Qt.createComponent("DeleteRootObjectInCreationComponentDerived.qml")
        c.createObject(null).setTestConditionsMet(test); // JS ownership, but it will be a RootObjectInCreation until finished beginCreate.
    }
}
