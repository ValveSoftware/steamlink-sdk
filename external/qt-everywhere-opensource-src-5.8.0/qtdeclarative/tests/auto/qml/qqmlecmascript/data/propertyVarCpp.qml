import QtQuick 2.0
import Qt.test 1.0

Item {
    id: testOwnership
    property int intProperty: 10
    property var varProperty: intProperty
    property var varProperty2: false
    property var varBound: varProperty + 5
    property int intBound: varProperty + 5
    property var jsobject: new vehicle(4)

    function vehicle(wheels) {
        this.wheels = wheels;
    }
}

