import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    objectName: "root"

    MySequenceConversionObject {
        id: msco
        objectName: "msco"
    }

    property bool success: true

    property variant intList
    property variant qrealList
    property variant boolList
    property variant stringList
    property variant urlList
    property variant qstringList

    // this test ensures that the "copy resource" codepaths work
    function testCopySequences() {
        success = true;

        // create "copy resource" sequences
        var jsIntList = msco.generateIntSequence();
        var jsQrealList = msco.generateQrealSequence();
        var jsBoolList = msco.generateBoolSequence();
        var jsStringList = msco.generateStringSequence();
        var jsUrlList = msco.generateUrlSequence();
        var jsQStringList = msco.generateQStringSequence();

        if (jsIntList.toString() != [1, 2, 3].toString())
            success = false;
        if (jsQrealList.toString() != [1.1, 2.2, 3.3].toString())
            success = false;
        if (jsBoolList.toString() != [true, false, true].toString())
            success = false;
        if (jsStringList.toString() != ["one", "two", "three"].toString())
            success = false;
        if (jsUrlList.toString() != ["http://www.example1.com", "http://www.example2.com", "http://www.example3.com"].toString())
            success = false;
        if (jsQStringList.toString() != ["one", "two", "three"].toString())
            success = false;

        // copy the sequence; should result in a new copy
        intList = jsIntList;
        qrealList = jsQrealList;
        boolList = jsBoolList;
        stringList = jsStringList;
        urlList = jsUrlList;
        qstringList = jsQStringList;

        // these operations shouldn't modify either variables - because
        // we don't handle writing to the intermediate variant at list[index]
        // for variant properties.
        intList[1] = 8;
        qrealList[1] = 8.8;
        boolList[1] = true;
        stringList[1] = "eight";
        urlList[1] = "http://www.example8.com";
        qstringList[1] = "eight";

        if (jsIntList[1] == 8)
            success = false;
        if (jsQrealList[1] == 8.8)
            success = false;
        if (jsBoolList[1] == true)
            success = false;
        if (jsStringList[1] == "eight")
            success = false;
        if (jsUrlList[1] == "http://www.example8.com")
            success = false;
        if (jsQStringList[1] == "eight")
            success = false;

        // assign a "copy resource" sequence to a QObject Q_PROPERTY
        msco.intListProperty = intList;
        msco.qrealListProperty = qrealList;
        msco.boolListProperty = boolList;
        msco.stringListProperty = stringList;
        msco.urlListProperty = urlList;
        msco.qstringListProperty = qstringList;

        if (msco.intListProperty.toString() != [1, 2, 3].toString())
            success = false;
        if (msco.qrealListProperty.toString() != [1.1, 2.2, 3.3].toString())
            success = false;
        if (msco.boolListProperty.toString() != [true, false, true].toString())
            success = false;
        if (msco.stringListProperty.toString() != ["one", "two", "three"].toString())
            success = false;
        if (msco.urlListProperty.toString() != ["http://www.example1.com", "http://www.example2.com", "http://www.example3.com"].toString())
            success = false;
        if (msco.qstringListProperty.toString() != ["one", "two", "three"].toString())
            success = false;

        // now modify the QObject Q_PROPERTY (reference resource) sequences - shouldn't modify the copy resource sequences.
        msco.intListProperty[2] = 9;
        msco.qrealListProperty[2] = 9.9;
        msco.boolListProperty[2] = false;
        msco.stringListProperty[2] = "nine";
        msco.urlListProperty[2] = "http://www.example9.com";
        msco.qstringListProperty[2] = "nine";

        if (intList[2] == 9)
            success = false;
        if (qrealList[2] == 9.9)
            success = false;
        if (boolList[2] == false)
            success = false;
        if (stringList[2] == "nine")
            success = false;
        if (urlList[2] == "http://www.example9.com")
            success = false;
        if (qstringList[2] == "nine")
            success = false;
    }

    property int intVal
    property real qrealVal
    property bool boolVal
    property string stringVal

    // this test ensures that indexed access works for copy resource sequences.
    function readSequenceCopyElements() {
        success = true;

        var jsIntList = msco.generateIntSequence();
        var jsQrealList = msco.generateQrealSequence();
        var jsBoolList = msco.generateBoolSequence();
        var jsStringList = msco.generateStringSequence();

        intVal = jsIntList[1];
        qrealVal = jsQrealList[1];
        boolVal = jsBoolList[1];
        stringVal = jsStringList[1];

        if (intVal != 2)
            success = false;
        if (qrealVal != 2.2)
            success = false;
        if (boolVal != false)
            success = false;
        if (stringVal != "two")
            success = false;
    }

    // this test ensures that equality works for copy resource sequences.
    function testEqualitySemantics() {
        success = true;

        var jsIntList = msco.generateIntSequence();
        var jsIntList2 = msco.generateIntSequence();

        if (jsIntList == jsIntList2) success = false;
        if (jsIntList != jsIntList) success = false;
    }

    // this test ensures that copy resource sequences can be passed as parameters
    function testCopyParameters() {
        success = false;

        var jsIntList = msco.generateIntSequence();
        success = msco.parameterEqualsGeneratedIntSequence(jsIntList);
        if (success == false) return;

        // here we construct something which should be converted to a copy sequence automatically.
        success = msco.parameterEqualsGeneratedIntSequence([1,2,3]);
    }

    // this test ensures that reference resource sequences are converted
    // to copy resource sequences when passed as parameters.
    function testReferenceParameters() {
        success = false;

        msco.intListProperty = msco.generateIntSequence();
        var jsIntList = msco.intListProperty
        success = msco.parameterEqualsGeneratedIntSequence(jsIntList);
        if (success == false) return;
    }
}
