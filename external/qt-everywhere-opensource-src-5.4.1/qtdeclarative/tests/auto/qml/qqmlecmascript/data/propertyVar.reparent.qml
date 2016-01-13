import QtQuick 2.0

Item {
    id: root
    objectName: "separateRootObject"
    property var vp

    function constructGarbage() {
        var retn = 1;
        var component = Qt.createComponent("PropertyVarOwnershipComponent.qml");
        if (component.status == Component.Ready) {
            retn = component.createObject(null); // has JavaScript ownership
        }
        return retn;
    }

    function assignVarProp() {
        vp = constructGarbage();
        gc();
    }

    function deassignVarProp() {
        vp = 2;
    }
}

