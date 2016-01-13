import QtQuick 2.0
import Qt.test 1.0

Item {
    id: testScarce

    property var varProperty

    property var canary: 4

    // constructs an Item which contains a scarce resource.
    function constructScarceObject() {
        var retn = 1;
        var component = Qt.createComponent("ScarceResourceVarComponent.qml");
        if (component.status == Component.Ready) {
            retn = component.createObject(null); // has JavaScript ownership
        }
        return retn;
    }

    function assignVarProperty() {
        varProperty = constructScarceObject();
        gc();
    }

    function deassignVarProperty() {
        varProperty = 2; // causes the original object to be garbage collected.
        gc();            // image should be detached; ep->sr should be empty!
    }
}
