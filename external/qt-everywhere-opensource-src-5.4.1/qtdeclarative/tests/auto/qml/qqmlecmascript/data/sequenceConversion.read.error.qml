import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    property int typeListLength: 0
    property variant typeList

    function performTest() {
        // we have NOT registered QList<NonRegisteredType> as a type
        typeListLength = msco.typeListProperty.length;
        typeList = msco.typeListProperty;
    }
}
