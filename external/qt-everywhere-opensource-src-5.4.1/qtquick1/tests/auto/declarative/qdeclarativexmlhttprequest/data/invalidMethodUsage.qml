import QtQuick 1.0

QtObject {
    property bool onreadystatechange: false
    property bool readyState: false
    property bool status: false
    property bool statusText: false
    property bool responseText: false
    property bool responseXML: false

    property bool open: false
    property bool setRequestHeader: false
    property bool send: false
    property bool abort: false
    property bool getResponseHeader: false
    property bool getAllResponseHeaders: false

    Component.onCompleted: {
        var o = 10;

        try {
            XMLHttpRequest.prototype.onreadystatechange
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            onreadystatechange = true;
        }
        try {
            XMLHttpRequest.prototype.readyState
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            readyState = true;
        }
        try {
            XMLHttpRequest.prototype.status
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            status = true;
        }
        try {
            XMLHttpRequest.prototype.statusText
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            statusText = true;
        }
        try {
            XMLHttpRequest.prototype.responseText
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            responseText = true;
        }
        try {
            XMLHttpRequest.prototype.responseXML
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            responseXML = true;
        }

        try {
            XMLHttpRequest.prototype.open.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            open = true;
        }

        try {
            XMLHttpRequest.prototype.setRequestHeader.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            setRequestHeader = true;
        }

        try {
            XMLHttpRequest.prototype.send.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            send = true;
        }

        try {
            XMLHttpRequest.prototype.abort.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            abort = true;
        }

        try {
            XMLHttpRequest.prototype.getResponseHeader.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            getResponseHeader = true;
        }

        try {
            XMLHttpRequest.prototype.getAllResponseHeaders.call(o);
        } catch (e) {
            if (!(e instanceof ReferenceError))
                return;

            if (e.message != "Not an XMLHttpRequest object")
                return;

            getAllResponseHeaders = true;
        }
    }
}
