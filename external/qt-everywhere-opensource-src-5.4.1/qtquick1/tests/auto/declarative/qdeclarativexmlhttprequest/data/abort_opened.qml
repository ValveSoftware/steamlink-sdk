import QtQuick 1.0

QtObject {
    property string url: "testdocument.html"

    property bool readyState: false
    property bool openedState: false

    property bool status: false
    property bool statusText: false
    property bool responseText: false
    property bool responseXML: false

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;
        x.abort();

        if (x.readyState == XMLHttpRequest.UNSENT)
            readyState = true;

        x.open("PUT", url);
        x.setRequestHeader("Accept-Language", "en-US");

        x.abort();

        x.open("GET", url);
        x.setRequestHeader("Accept-Language", "en-US");

        if (x.readyState  == XMLHttpRequest.OPENED)
            openedState = true;

        try {
            var a = x.status;
        } catch (error) {
            if (error.code == DOMException.INVALID_STATE_ERR)
                status = true;
        }
        try {
            var a = x.statusText;
        } catch (error) {
            if (error.code == DOMException.INVALID_STATE_ERR)
                statusText = true;
        }
        responseText = (x.responseText == "");
        responseXML = (x.responseXML == null);

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }


        x.send()
    }
}

