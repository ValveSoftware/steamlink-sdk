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
    property bool finished: false

    function testIntSequence() {
        msco.intListProperty = [ 0, 1, 2, 3, 4, 5, 6, 7 ];
        worker.sendSequence(msco.intListProperty);
    }

    function testQrealSequence() {
        msco.qrealListProperty = [ 0.1, 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1 ];
        worker.sendSequence(msco.qrealListProperty);
    }

    function testBoolSequence() {
        msco.boolListProperty = [ false, true, true, false, false, true, false, true ];
        worker.sendSequence(msco.boolListProperty);
    }

    function testStringSequence() {
        msco.stringListProperty = [ "one", "two", "three", "four" ];
        worker.sendSequence(msco.stringListProperty);
    }

    function testQStringSequence() {
        msco.qstringListProperty = [ "one", "two", "three", "four" ];
        worker.sendSequence(msco.qstringListProperty);
    }

    function testUrlSequence() {
        msco.urlListProperty = [ "www.example1.com", "www.example2.com", "www.example3.com", "www.example4.com" ];
        worker.sendSequence(msco.urlListProperty);
    }

    function testVariantSequence() {
        msco.variantListProperty = [ "one", true, 3, "four" ];
        worker.sendSequence(msco.variantListProperty);
    }

    WorkerScript {
        id: worker
        source: "threadScript.js"

        property variant expected
        property variant response

        function sendSequence(seq) {
            root.success = false;
            root.finished = false;
            worker.expected = seq;
            worker.sendMessage(seq);
        }

        onMessage: {
            worker.response = messageObject;
            if (worker.response.toString() == worker.expected.toString())
                root.success = true;
            else
                root.success = false;
            root.finished = true;
        }
    }
}
