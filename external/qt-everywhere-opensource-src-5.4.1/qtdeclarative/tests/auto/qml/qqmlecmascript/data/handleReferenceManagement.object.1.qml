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

    function createReference() {
        // generate the objects
        first = cro.generate(cro); // has parent, so won't be collected
        second = cro.generate();   // no parent, but will be kept alive by first's reference
        first.addReference(second);

        // remove top level references
        first = cro;
        second = cro;
    }
}
