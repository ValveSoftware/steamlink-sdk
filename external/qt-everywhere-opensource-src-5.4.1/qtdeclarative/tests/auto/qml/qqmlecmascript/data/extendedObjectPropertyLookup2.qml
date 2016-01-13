import Qt.test 1.0
import QtQuick 2.0

QtObject {
    id: root
    property MyExtendedObject a;
    a: MyExtendedObject {
        id: obj
        extendedProperty: 42;
    }
    function getValue() {
        return obj.extendedProperty;
    }
}
