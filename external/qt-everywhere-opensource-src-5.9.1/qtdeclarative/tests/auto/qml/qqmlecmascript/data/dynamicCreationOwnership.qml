import QtQuick 2.0
import Qt.test 1.0

Item {
    id: obj
    objectName: "obj"

    MyDynamicCreationDestructionObject {
        id: mdcdo
        objectName: "mdcdo"
    }

    function dynamicallyCreateJsOwnedObject() {
        mdcdo.createNew();
    }

    function performGc() {
        gc();
    }
}
