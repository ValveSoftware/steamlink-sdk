import QtQuick 2.0

QtObject {
    property bool dataOK: false
    property bool test: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;
        x.open("GET", "testdocument.html");
        x.setRequestHeader("Accept-Language","en-US");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send();

        try {
            x.send()
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                test = true;
        }
    }
}
