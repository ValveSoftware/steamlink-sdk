import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    property bool success: false

    function verifyExpected(array, idx) {
        for (var i = 0; i < idx; ++i) {
            if (array[i] != i) {
                return false;
            }
        }
        return true;
    }

    function indexedAccess() {
        success = true;

        msco.intListProperty = [ 0, 1, 2, 3, 4 ];
        var expectedLength = msco.intListProperty.length;
        var maxIndex = msco.maxIndex;
        var tooBigIndex = msco.tooBigIndex;
        var negativeIndex = msco.negativeIndex;

        // shouldn't be able to set the length > maxIndex.
        msco.intListProperty.length = tooBigIndex;
        if (msco.intListProperty.length != expectedLength)
            success = false;
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;

        // shouldn't be able to set any index > maxIndex.
        msco.intListProperty[tooBigIndex] = 12;
        if (msco.intListProperty.length != expectedLength)
            success = false;
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;

        // shouldn't be able to access any index > maxIndex.
        var valueAtTBI = msco.intListProperty[tooBigIndex];
        if (valueAtTBI != undefined)
            success = false;
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;

        // shouldn't be able to set the length to < 0
        msco.intListProperty.length = negativeIndex;
        if (msco.intListProperty.length != expectedLength)
            success = false; // shouldn't have changed.
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;

        // shouldn't be able to set any index < 0.
        msco.intListProperty[negativeIndex] = 12;
        if (msco.intListProperty.length != expectedLength)
            success = false;
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;

        // shouldn't be able to access any index < 0.
        var valueAtNI = msco.intListProperty[negativeIndex];
        if (valueAtNI != undefined)
            success = false;
        if (!verifyExpected(msco.intListProperty, 4))
            success = false;
    }

    function indexOf() {
        if (msco.qstringListProperty.length != 4)
            success = false;
        if (msco.qstringListProperty.indexOf("first") != 0)
            success = false;
        if (msco.qstringListProperty.indexOf("second") != 1)
            success = false;
        if (msco.qstringListProperty.indexOf("third") != 2)
            success = false;
        if (msco.qstringListProperty.indexOf("fourth") != 3)
            success = false;
    }
}
