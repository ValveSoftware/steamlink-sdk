import QtQuick 1.0

QtObject {
    property string url
    property string expectedStatus

    property bool unsentException: false;
    property bool openedException: false;
    property bool sentException: false;

    property bool headersReceived: false
    property bool loading: false
    property bool done: false

    property bool resetException: false

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        try {
            var a = x.statusText;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                unsentException = true;
        }

        x.open("GET", url);
        x.setRequestHeader("Accept-Language", "en-US");

        try {
            var a = x.statusText;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                openedException = true;
        }

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                if (x.statusText == expectedStatus)
                    headersReceived = true;
            } else if (x.readyState == XMLHttpRequest.LOADING) {
                if (x.statusText == expectedStatus)
                    loading = true;
            } else if (x.readyState == XMLHttpRequest.DONE) {
                if (x.statusText == expectedStatus)
                    done = true;

                if (expectedStatus != "OK") {
                    dataOK = (x.responseText == "");
                } else {
                    dataOK = (x.responseText == "QML Rocks!\n");
                }

                x.open("GET", url);
                x.setRequestHeader("Accept-Language", "en-US");

                try {
                    var a = x.statusText;
                } catch (e) {
                    if (e.code == DOMException.INVALID_STATE_ERR)
                        resetException = true;
                }

            }
        }

        x.send()

        try {
            var a = x.statusText;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                sentException = true;
        }
    }
}
