import QtQuick 1.0

QtObject {
    property string url
    property int expectedStatus

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
            var a = x.status;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                unsentException = true;
        }

        x.open("GET", url);
        x.setRequestHeader("Accept-Language", "en-US");

        try {
            var a = x.status;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                openedException = true;
        }

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                if (x.status == expectedStatus)
                    headersReceived = true;
            } else if (x.readyState == XMLHttpRequest.LOADING) {
                if (x.status == expectedStatus)
                    loading = true;
            } else if (x.readyState == XMLHttpRequest.DONE) {
                if (x.status == expectedStatus)
                    done = true;

                if (expectedStatus == 404) {
                    dataOK = (x.responseText == "");
                } else {
                    dataOK = (x.responseText == "QML Rocks!\n");
                }

                x.open("GET", url);
                x.setRequestHeader("Accept-Language", "en-US");

                try {
                    var a = x.status;
                } catch (e) {
                    if (e.code == DOMException.INVALID_STATE_ERR)
                        resetException = true;
                }

            }
        }

        x.send()

        try {
            var a = x.status;
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                sentException = true;
        }
    }
}
