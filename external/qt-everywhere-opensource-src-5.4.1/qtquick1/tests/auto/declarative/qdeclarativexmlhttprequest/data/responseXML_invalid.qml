import QtQuick 1.0

QtObject {
    property bool xmlNull: false
    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "testdocument.html");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "QML Rocks!\n");
                xmlNull = (x.responseXML == null);
            }
        }


        x.send()
    }
}

