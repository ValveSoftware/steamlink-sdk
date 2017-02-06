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

    property variant intList
    property variant qrealList
    property variant boolList
    property variant stringList

    function indexedAccess() {
        intList = msco.intListProperty;
        var jsIntList = msco.intListProperty;
        qrealList = msco.qrealListProperty;
        var jsQrealList = msco.qrealListProperty;
        boolList = msco.boolListProperty;
        var jsBoolList = msco.boolListProperty;
        stringList = msco.stringListProperty;
        var jsStringList = msco.stringListProperty;

        // Three cases: direct property modification, variant copy modification, js var reference modification.
        // Only the first and third should "write back" to the original QObject Q_PROPERTY; the second one
        // should have no effect whatsoever to maintain "property variant" semantics (see e.g., valuetype).
        success = true;

        msco.intListProperty[1] = 33;
        if (msco.intListProperty[1] != 33) success = false;                  // ensure write back
        intList[1] = 44;
        if (intList[1] == 44) success = false;                               // ensure no effect
        jsIntList[1] = 55;
        if (jsIntList[1] != 55
                || jsIntList[1] != msco.intListProperty[1]) success = false; // ensure write back

        msco.qrealListProperty[1] = 33.3;
        if (msco.qrealListProperty[1] != 33.3) success = false;               // ensure write back
        qrealList[1] = 44.4;
        if (qrealList[1] == 44.4) success = false;                            // ensure no effect
        jsQrealList[1] = 55.5;
        if (jsQrealList[1] != 55.5
                || jsQrealList[1] != msco.qrealListProperty[1]) success = false; // ensure write back

        msco.boolListProperty[1] = true;
        if (msco.boolListProperty[1] != true) success = false;           // ensure write back
        boolList[1] = true;
        if (boolList[1] != false) success = false;                       // ensure no effect
        jsBoolList[1] = false;
        if (jsBoolList[1] != false
                || jsBoolList[1] != msco.boolListProperty[1]) success = false; // ensure write back

        msco.stringListProperty[1] = "changed";
        if (msco.stringListProperty[1] != "changed") success = false;    // ensure write back
        stringList[1] = "changed";
        if (stringList[1] != "second") success = false;                  // ensure no effect
        jsStringList[1] = "different";
        if (jsStringList[1] != "different"
                || jsStringList[1] != msco.stringListProperty[1]) success = false; // ensure write back
    }

    function arrayOperations() {
        success = true;
        var expected = 0;
        var expectedStr = "";

        // ecma262r3 defines array as implementing Length and Put.  Test put here.
        msco.intListProperty.asdf = 5; // shouldn't work, only indexes are valid names.
        if (msco.intListProperty.asdf == 5) success = false;
        msco.intListProperty[3] = 38;   // should work.
        if (msco.intListProperty[3] != 38) success = false;
        msco.intListProperty[199] = 200; // should work, and should set length to 200.
        if (msco.intListProperty[199] != 200) success = false;
        if (msco.intListProperty.length != 200) success = false;

        // test indexed deleter
        msco.intListProperty = [ 1, 2, 3, 4, 5 ];
        delete msco.intListProperty[-1];
        expected = [ 1, 2, 3, 4, 5 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;
        delete msco.intListProperty[0];
        expected = [ 0, 2, 3, 4, 5 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;
        delete msco.intListProperty[2];
        expected = [ 0, 2, 0, 4, 5 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;
        delete msco.intListProperty[7];
        expected = [ 0, 2, 0, 4, 5 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;

        // other operations are defined on the array prototype; see if they work.

        // splice
        msco.intListProperty = [ 0, 1, 2, 3, 4, 5, 6, 7 ];
        msco.intListProperty.splice(1,3, 33, 44, 55, 66);
        expected = [ 0, 33, 44, 55, 66, 4, 5, 6, 7 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;
        msco.intListProperty = [ 0, 1, 2, 3, 4, 5, 6, 7 ];
        msco.intListProperty.splice(1, 3);
        expected = [ 0, 4, 5, 6, 7 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;

        msco.qrealListProperty = [ 0.1, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1 ];
        msco.qrealListProperty.splice(1,3, 33.33, 44.44, 55.55, 66.66);
        expected = [ 0.1, 33.33, 44.44, 55.55, 66.66, 4.1, 5.1, 6.1, 7.1 ];
        if (msco.qrealListProperty.toString() != expected.toString()) success = false;
        msco.qrealListProperty = [ 0.1, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1 ];
        msco.qrealListProperty.splice(1,3);
        expected = [ 0.1, 4.1, 5.1, 6.1, 7.1 ];
        if (msco.qrealListProperty.toString() != expected.toString()) success = false;

        msco.boolListProperty = [ false, true, true, false, false, true, false, true ];
        msco.boolListProperty.splice(1,3, false, true, false, false);
        expected = [ false, false, true, false, false, false, true, false, true ];
        if (msco.boolListProperty.toString() != expected.toString()) success = false;
        msco.boolListProperty = [ false, true, true, false, false, true, false, true ];
        msco.boolListProperty.splice(1,3);
        expected = [ false, false, true, false, true ];
        if (msco.boolListProperty.toString() != expected.toString()) success = false;

        msco.stringListProperty = [ "one", "two", "three", "four", "five", "six", "seven", "eight" ];
        msco.stringListProperty.splice(1,3, "nine", "ten", "eleven", "twelve");
        expected = [ "one", "nine", "ten", "eleven", "twelve", "five", "six", "seven", "eight" ];
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        msco.stringListProperty = [ "one", "two", "three", "four", "five", "six", "seven", "eight" ];
        msco.stringListProperty.splice(0,3);
        expected = [ "four", "five", "six", "seven", "eight" ];
        if (msco.stringListProperty.toString() != expected.toString()) success = false;

        // pop
        msco.intListProperty = [ 0, 1, 2, 3, 4, 5, 6, 7 ];
        var poppedVal = msco.intListProperty.pop();
        expected = [ 0, 1, 2, 3, 4, 5, 6 ];
        if (msco.intListProperty.toString() != expected.toString()) success = false;
        expected = 7;
        if (poppedVal != expected) success = false;

        // push
        msco.stringListProperty = [ "one", "two" ]
        msco.stringListProperty.push("three")
        expected = [ "one", "two", "three" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        msco.stringListProperty.push("four", "five")
        expected = [ "one", "two", "three", "four", "five" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;

        // concat
        msco.stringListProperty = [ "one", "two" ]
        stringList = [ "hello", "world" ]
        stringList = stringList.concat(msco.stringListProperty)
        expected = [ "hello", "world", "one", "two" ]
        if (stringList.length != expected.length) {
            success = false;
        } else {
            for (var i = 0; i < stringList.length; ++i) {
                if (stringList[i] != expected[i]) {
                    success = false;
                    break;
                }
            }
        }

        // unshift
        msco.stringListProperty = [ "one", "two" ]
        var unshiftedVal = msco.stringListProperty.unshift("zero")
        expected = [ "zero", "one", "two" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        expected = 3
        if (msco.stringListProperty.length != expected) success = false
        msco.stringListProperty = [ ]
        msco.stringListProperty.unshift("zero", "one")
        expected = [ "zero", "one" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        expected = 2
        if (msco.stringListProperty.length != expected) success = false

        // shift
        msco.stringListProperty = [ "one", "two", "three" ]
        var shiftVal = msco.stringListProperty.shift()
        expected = [ "two", "three" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        expected = "one"
        if (shiftVal != expected) success = false
        shiftVal = msco.stringListProperty.shift()
        expected = [ "three" ]
        if (msco.stringListProperty.toString() != expected.toString()) success = false;
        expected = "two"
        if (shiftVal != expected) success = false
        shiftVal = msco.stringListProperty.shift()
        expected = 0
        if (msco.stringListProperty.length != expected) success = false;
        expected = "three"
        if (shiftVal != expected) success = false
        shiftVal = msco.stringListProperty.shift()
        expected = 0
        if (msco.stringListProperty.length != expected) success = false;
        expected = undefined
        if (shiftVal != expected) success = false
    }

    property variant variantList: [ 1, 2, 3, 4, 5 ];
    property variant variantList2: [ 1, 2, 3, 4, 5 ];
    function testEqualitySemantics() {
        // ensure equality semantics match JS array equality semantics
        success = true;

        msco.intListProperty = [ 1, 2, 3, 4, 5 ];
        msco.intListProperty2 = [ 1, 2, 3, 4, 5 ];
        var jsIntList = [ 1, 2, 3, 4, 5 ];
        var jsIntList2 = [ 1, 2, 3, 4, 5 ];

        if (jsIntList != jsIntList) success = false;
        if (jsIntList == jsIntList2) success = false;
        if (jsIntList == msco.intListProperty) success = false;
        if (jsIntList == variantList) success = false;

        if (msco.intListProperty != msco.intListProperty) success = false;
        if (msco.intListProperty == msco.intListProperty2) success = false;
        if (msco.intListProperty == jsIntList) success = false;
        if (msco.intListProperty == variantList) success = false;

        if (variantList == variantList) return false;
        if (variantList == variantList2) return false;
        if (variantList == msco.intListProperty) return false;
        if (variantList == jsIntList) return false;

        if ((jsIntList == jsIntList2) != (jsIntList == msco.intListProperty)) success = false;
        if ((jsIntList == jsIntList2) != (msco.intListProperty == msco.intListProperty2)) success = false;
        if ((jsIntList == jsIntList) != (msco.intListProperty == msco.intListProperty)) success = false;
        if ((jsIntList == variantList) != (msco.intListProperty == variantList)) success = false;
        if ((variantList == jsIntList) != (variantList == msco.intListProperty)) success = false;
        if ((msco.intListProperty == variantList) != (variantList == msco.intListProperty)) success = false;
    }

    property bool referenceDeletion: false
    function testReferenceDeletion() {
        referenceDeletion = true;
        var testObj = msco.generateTestObject();
        testObj.intListProperty = [1, 2, 3, 4, 5];
        var testSequence = testObj.intListProperty;
        var prevString = testSequence.toString();
        var prevValueOf = testSequence.valueOf();
        var prevLength = testSequence.length;
        msco.deleteTestObject(testObj); // delete referenced object.
        if (testSequence.toString() == prevString) referenceDeletion = false;
        if (testSequence.valueOf() == prevValueOf) referenceDeletion = false;
        if (testSequence.length == prevLength) referenceDeletion = false;
    }
}
