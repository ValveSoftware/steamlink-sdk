import QtQuick 2.0
import Qt.test 1.0

Item {
    id: testInheritance

    property var varProperty

    function constructGarbage() {
        var retn = 1;
        var component = Qt.createComponent("PropertyVarInheritanceComponent.qml");
        if (component.status == Component.Ready) {
            retn = component.createObject(null); // has JavaScript ownership
        }
        return retn;
    }

    function assignCircular() {
        varProperty = constructGarbage();
        gc();
    }

    function deassignCircular() {
        varProperty = 2;
    }

    function assignThenDeassign() {
        varProperty = constructGarbage();
        varProperty = 2;
        gc();
    }
}

