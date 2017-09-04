import QtQuick 2.0

QtObject {
    property string url

    property bool dataOK: false
    property bool headerOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;
        x.open("OPTIONS", url);

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                headerOK = (x.getResponseHeader("Allow") == "GET,HEAD,POST,OPTIONS,TRACE");
            } else if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "");
            }
        }

        x.send("My Sent Data");
    }
}
