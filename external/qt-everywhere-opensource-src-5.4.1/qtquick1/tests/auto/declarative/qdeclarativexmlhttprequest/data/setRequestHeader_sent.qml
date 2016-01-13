import QtQuick 1.0

QtObject {
    property string url
    property bool test: false

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", url);
        x.setRequestHeader("Accept-Language","en-US");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send();

        try {
            x.setRequestHeader("Test-header", "value");
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                test = true;
        }
    }
}

