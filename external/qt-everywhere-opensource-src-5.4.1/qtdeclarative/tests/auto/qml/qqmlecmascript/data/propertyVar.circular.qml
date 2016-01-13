import QtQuick 2.0
import Qt.test 1.0

Item {
    id: testCircular

    property var varProperty
    property variant canaryResource
    property int canaryInt

    function constructGarbage() {
        var retn = 1;
        var component = Qt.createComponent("PropertyVarCircularComponent.qml");
        if (component.status == Component.Ready) {
            retn = component.createObject(null); // has JavaScript ownership
        }
        return retn;
    }

    function deassignCanaryResource() {
        canaryResource = 1;
    }

    function assignCircular() {
        varProperty = constructGarbage();
        canaryResource = varProperty.vp.vp.vp.vp.memoryHog;
        canaryInt = varProperty.vp.vp.vp.vp.fifthCanary; // == 5
        gc();
    }

    function deassignCircular() {
        canaryInt = 2;
        varProperty = 2;
    }

    function assignThenDeassign() {
        varProperty = constructGarbage();
        varProperty = 2;
        gc();
    }
}

