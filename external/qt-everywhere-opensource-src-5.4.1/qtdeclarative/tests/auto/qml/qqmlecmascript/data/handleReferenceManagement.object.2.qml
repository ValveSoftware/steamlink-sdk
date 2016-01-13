import QtQuick 2.0
import Qt.test 1.0

Item {
    id: obj
    objectName: "obj"

    property CircularReferenceObject first
    property CircularReferenceObject second


    CircularReferenceObject {
        id: cro
        objectName: "cro"
    }

    function circularReference() {
        // generate the circularly referential pair - they should still be collected
        first = cro.generate();  // no parent, so should be collected
        second = cro.generate(); // no parent, so should be collected
        first.addReference(second);
        second.addReference(first);

        // remove top level references
        first = cro;
        second = cro;
    }
}
