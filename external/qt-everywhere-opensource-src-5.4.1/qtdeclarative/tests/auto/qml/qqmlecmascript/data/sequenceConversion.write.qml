import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    property bool success

    function writeSequences() {
        success = true;

        var intList = [ 9, 8, 7, 6 ];
        msco.intListProperty = intList;
        var qrealList = [ 9.9, 8.8, 7.7, 6.6 ];
        msco.qrealListProperty = qrealList;
        var boolList = [ false, false, false, true ];
        msco.boolListProperty = boolList;
        var stringList = [ "nine", "eight", "seven", "six" ]
        msco.stringListProperty = stringList;
        var urlList = [ "http://www.example9.com", "http://www.example8.com", "http://www.example7.com", "http://www.example6.com" ]
        msco.urlListProperty = urlList;
        var qstringList = [ "nine", "eight", "seven", "six" ]
        msco.qstringListProperty = qstringList;

        if (msco.intListProperty[0] != 9 || msco.intListProperty[1] != 8 || msco.intListProperty[2] != 7 || msco.intListProperty[3] != 6)
            success = false;
        if (msco.qrealListProperty[0] != 9.9 || msco.qrealListProperty[1] != 8.8 || msco.qrealListProperty[2] != 7.7 || msco.qrealListProperty[3] != 6.6)
            success = false;
        if (msco.boolListProperty[0] != false || msco.boolListProperty[1] != false || msco.boolListProperty[2] != false || msco.boolListProperty[3] != true)
            success = false;
        if (msco.stringListProperty[0] != "nine" || msco.stringListProperty[1] != "eight" || msco.stringListProperty[2] != "seven" || msco.stringListProperty[3] != "six")
            success = false;
        if (msco.urlListProperty[0] != "http://www.example9.com" || msco.urlListProperty[1] != "http://www.example8.com" || msco.urlListProperty[2] != "http://www.example7.com" || msco.urlListProperty[3] != "http://www.example6.com")
            success = false;
        if (msco.qstringListProperty[0] != "nine" || msco.qstringListProperty[1] != "eight" || msco.qstringListProperty[2] != "seven" || msco.qstringListProperty[3] != "six")
            success = false;
    }

    function writeSequenceElements() {
        // set up initial conditions.
        writeSequences();
        success = true;

        // element set.
        msco.intListProperty[3] = 2;
        msco.qrealListProperty[3] = 2.2;
        msco.boolListProperty[3] = false;
        msco.stringListProperty[3] = "changed";
        msco.urlListProperty[3] = "http://www.examplechanged.com";
        msco.qstringListProperty[3] = "changed";

        if (msco.intListProperty[0] != 9 || msco.intListProperty[1] != 8 || msco.intListProperty[2] != 7 || msco.intListProperty[3] != 2)
            success = false;
        if (msco.qrealListProperty[0] != 9.9 || msco.qrealListProperty[1] != 8.8 || msco.qrealListProperty[2] != 7.7 || msco.qrealListProperty[3] != 2.2)
            success = false;
        if (msco.boolListProperty[0] != false || msco.boolListProperty[1] != false || msco.boolListProperty[2] != false || msco.boolListProperty[3] != false)
            success = false;
        if (msco.stringListProperty[0] != "nine" || msco.stringListProperty[1] != "eight" || msco.stringListProperty[2] != "seven" || msco.stringListProperty[3] != "changed")
            success = false;
        if (msco.urlListProperty[0] != "http://www.example9.com" || msco.urlListProperty[1] != "http://www.example8.com" || msco.urlListProperty[2] != "http://www.example7.com" || msco.urlListProperty[3] != "http://www.examplechanged.com")
            success = false;
        if (msco.qstringListProperty[0] != "nine" || msco.qstringListProperty[1] != "eight" || msco.qstringListProperty[2] != "seven" || msco.qstringListProperty[3] != "changed")
            success = false;
    }

    function writeOtherElements() {
        success = true;
        var jsIntList = [1, 2, 3, 4, 5];
        msco.intListProperty = [1, 2, 3, 4, 5];

        jsIntList[8] = 8;
        msco.intListProperty[8] = 8;
        if (jsIntList[8] != msco.intListProperty[8])
            success = false;
        if (jsIntList.length != msco.intListProperty.length)
            success = false;

        // NOTE: we can't exactly match the spec here -- we fill the sequence with a default (rather than empty) value
        if (msco.intListProperty[5] != 0 || msco.intListProperty[6] != 0 || msco.intListProperty[7] != 0)
            success = false;

        // should have no effect
        var currLength = jsIntList.length;
        jsIntList.someThing = 9;
        msco.intListProperty.someThing = 9;
        if (msco.intListProperty.length != currLength)
            success = false;
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
        testSequence[4] = 5; // shouldn't work, since referenced object no longer exists.
        if (testSequence[4] == 5)
            referenceDeletion = false;
    }
}
