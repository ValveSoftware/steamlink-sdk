import QtQuick 1.0

QtObject {
    property string url

    property bool unsentException: false
    property bool openedException: false

    property bool readyState: false
    property bool openedState: false

    property bool headersReceivedState: false
    property bool headersReceivedHeader: false

    property bool doneState: false
    property bool doneHeader: false

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        try {
            x.getResponseHeader("Test-Header");
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                unsentException = true;
        }

        if (x.readyState == XMLHttpRequest.UNSENT)
            readyState = true;

        x.open("GET", url);
        x.setRequestHeader("Accept-Language", "en-US");

        if (x.readyState  == XMLHttpRequest.OPENED)
            openedState = true;

        try {
            x.getResponseHeader("Test-Header");
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                openedException = true;
        }

        var headers = "connection: close\r\ncontent-type: text/html; charset=UTF-8\r\ntest-header: TestValue\r\nmultitest-header: TestValue, SecondTestValue\r\ncontent-length: 11";

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                headersReceivedState = true;

                headersReceivedHeader = (x.getAllResponseHeaders() == headers);
            } else if (x.readyState == XMLHttpRequest.DONE) {
                doneState = headersReceivedState && true;

                doneHeader = (x.getAllResponseHeaders() == headers);
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send()
    }
}


