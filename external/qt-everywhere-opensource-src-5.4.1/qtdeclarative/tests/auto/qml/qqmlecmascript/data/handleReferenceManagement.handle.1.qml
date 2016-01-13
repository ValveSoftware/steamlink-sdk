import QtQuick 2.0
import Qt.test 1.0

Item {
    id: obj
    objectName: "obj"
    property CircularReferenceHandle first
    property CircularReferenceHandle second

    CircularReferenceHandle {
        id: crh
        objectName: "crh"
    }

    function createReference() {
        first = crh.generate(crh);
        second = crh.generate(crh);
        // NOTE: manually add reference from first to second
        // in unit test prior reparenting and gc.
    }
}
