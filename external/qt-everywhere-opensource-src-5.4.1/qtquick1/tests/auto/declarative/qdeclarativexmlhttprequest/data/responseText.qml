import QtQuick 1.0

QtObject {
    property string url
    property string expectedText

    property bool unsent: false
    property bool opened: false
    property bool sent: false
    property bool headersReceived: false

    property bool loading: false
    property bool done: false

    property bool reset: false

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        unsent = (x.responseText == "");

        x.open("GET", url);
        x.setRequestHeader("Accept-Language", "en-US");

        opened = (x.responseText == "");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                headersReceived = (x.responseText == "");
            } else if (x.readyState == XMLHttpRequest.LOADING) {
                if (x.responseText == expectedText)
                    loading = true;
            } else if (x.readyState == XMLHttpRequest.DONE) {
                if (x.responseText == expectedText)
                    done = true;

                dataOK = (x.responseText == expectedText);

                x.open("GET", url);
                x.setRequestHeader("Accept-Language", "en-US");

                reset = (x.responseText == "");
            }
        }

        x.send()

        sent = (x.responseText == "");
    }
}

