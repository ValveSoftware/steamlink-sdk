import QtQuick 2.0
import Qt.test 1.0

PropertyVarCircularComponent {
    id: inheritanceComponent
    property int inheritanceIntProperty: 6
    property var inheritanceVarProperty

    function constructGarbage() {
        var retn = 1;
        var component = Qt.createComponent("PropertyVarCircularComponent2.qml");
        if (component.status == Component.Ready) {
            retn = component.createObject(null); // has JavaScript ownership
        }
        return retn;
    }

    Component.onCompleted: {
        inheritanceVarProperty = constructGarbage();
        gc();
    }
}
