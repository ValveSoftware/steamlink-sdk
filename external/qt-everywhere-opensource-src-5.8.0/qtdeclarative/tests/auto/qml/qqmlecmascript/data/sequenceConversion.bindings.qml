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
        intListProperty: msco.intListProperty
    }

    property variant boundSequence: msco.intListProperty
    property int boundElement: msco.intListProperty[3]
    property variant boundSequenceTwo: mscoTwo.intListProperty

    Component.onCompleted: {
        msco.intListProperty[3] = 12;
        mscoTwo.intListProperty[4] = 14;
    }
}
