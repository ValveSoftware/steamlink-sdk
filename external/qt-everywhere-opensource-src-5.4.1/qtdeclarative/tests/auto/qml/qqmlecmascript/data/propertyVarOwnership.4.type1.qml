import QtQuick 2.0

// Has a self reference in selfRef, and a reference to propertyVarOwnership.4.qml in creatorRef
Item {
    id: root

    property var creatorRef
    property var selfRef
    property var object

    property int dummy: 10
    property bool test: false

    Component.onCompleted: {
        selfRef = root;

        var c = Qt.createComponent("propertyVarOwnership.4.type2.qml");
        object = c.createObject();
        object.creatorRef = root;

        test = true;
    }
}
