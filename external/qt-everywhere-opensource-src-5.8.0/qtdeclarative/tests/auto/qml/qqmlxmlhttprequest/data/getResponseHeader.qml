import QtQuick 2.0

QtObject {
    property string url

    property bool unsentException: false
    property bool openedException: false

    property bool readyState: false
    property bool openedState: false

    property bool headersReceivedState: false
    property bool headersReceivedNullHeader: false
    property bool headersReceivedValidHeader: false
    property bool headersReceivedMultiValidHeader: false
    property bool headersReceivedCookieHeader: false

    property bool doneState: false
    property bool doneNullHeader: false
    property bool doneValidHeader: false
    property bool doneMultiValidHeader: false
    property bool doneCookieHeader: false

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

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                headersReceivedState = true;

                headersReceivedNullHeader = (x.getResponseHeader("Nonexistant-header") == "");
                headersReceivedValidHeader = (x.getResponseHeader("Test-HEAder") == "TestValue");
                headersReceivedMultiValidHeader = (x.getResponseHeader("MultiTest-HEAder") == "TestValue, SecondTestValue");
                headersReceivedCookieHeader = (x.getResponseHeader("Set-Cookie") == "" && x.getResponseHeader("Set-Cookie2") == "");
            } else if (x.readyState == XMLHttpRequest.DONE) {
                doneState = headersReceivedState && true;

                doneNullHeader = (x.getResponseHeader("Nonexistant-header") == "");
                doneValidHeader = (x.getResponseHeader("Test-HEAder") == "TestValue");
                doneMultiValidHeader = (x.getResponseHeader("MultiTest-HEAder") == "TestValue, SecondTestValue");
                doneCookieHeader = (x.getResponseHeader("Set-Cookie") == "" && x.getResponseHeader("Set-Cookie2") == "");
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send()
    }
}


