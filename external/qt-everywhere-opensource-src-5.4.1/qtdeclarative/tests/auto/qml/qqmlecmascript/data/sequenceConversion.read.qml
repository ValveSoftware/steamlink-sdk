import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    property int intListLength: 0
    property variant intList
    property int qrealListLength: 0
    property variant qrealList
    property int boolListLength: 0
    property variant boolList
    property int stringListLength: 0
    property variant stringList
    property int urlListLength: 0
    property variant urlList
    property int qstringListLength: 0
    property variant qstringList

    function readSequences() {
        intListLength = msco.intListProperty.length;
        intList = msco.intListProperty;
        qrealListLength = msco.qrealListProperty.length;
        qrealList = msco.qrealListProperty;
        boolListLength = msco.boolListProperty.length;
        boolList = msco.boolListProperty;
        stringListLength = msco.stringListProperty.length;
        stringList = msco.stringListProperty;
        urlListLength = msco.urlListProperty.length;
        urlList = msco.urlListProperty;
        qstringListLength = msco.qstringListProperty.length;
        qstringList = msco.qstringListProperty;
    }

    property int intVal
    property real qrealVal
    property bool boolVal
    property string stringVal
    property url urlVal
    property string qstringVal

    function readSequenceElements() {
        intVal = msco.intListProperty[1];
        qrealVal = msco.qrealListProperty[1];
        boolVal = msco.boolListProperty[1];
        stringVal = msco.stringListProperty[1];
        urlVal = msco.urlListProperty[1];
        qstringVal = msco.qstringListProperty[1];
    }

    property bool enumerationMatches
    function enumerateSequenceElements() {
        var jsIntList = [1, 2, 3, 4, 5];
        msco.intListProperty = [1, 2, 3, 4, 5];

        var jsIntListProps = []
        var seqIntListProps = []

        enumerationMatches = true;
        for (var i in jsIntList) {
            jsIntListProps.push(i);
            if (jsIntList[i] != msco.intListProperty[i]) {
                enumerationMatches = false;
            }
        }
        for (var j in msco.intListProperty) {
            seqIntListProps.push(j);
            if (jsIntList[j] != msco.intListProperty[j]) {
                enumerationMatches = false;
            }
        }

        if (jsIntListProps.length != seqIntListProps.length) {
            enumerationMatches = false;
        }

        var emptyList = [];
        msco.stringListProperty = []
        if (emptyList.toString() != msco.stringListProperty.toString()) {
            enumerationMatches = false;
        }
        if (emptyList.valueOf() != msco.stringListProperty.valueOf()) {
            enumerationMatches = false;
        }
    }

    property bool referenceDeletion: false
    function testReferenceDeletion() {
        referenceDeletion = true;
        var testObj = msco.generateTestObject();
        testObj.intListProperty = [1, 2, 3, 4, 5];
        var testSequence = testObj.intListProperty;
        if (testSequence[4] != 5)
            referenceDeletion = false;
        msco.deleteTestObject(testObj); // delete referenced object.
        if (testSequence[4] == 5)
            referenceDeletion = false;
    }
}
