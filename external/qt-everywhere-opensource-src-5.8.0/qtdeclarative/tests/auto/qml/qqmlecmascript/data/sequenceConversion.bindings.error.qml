import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
        intListProperty: [ 1, 2, 3, 6, 7 ]
    }

    MySequenceConversionObject {
        id: mscoTwo
        objectName: "mscoTwo"
        boolListProperty: msco.intListProperty
    }
}
