import QtQuick 1.0

QtObject {
    property int unsent: XMLHttpRequest.UNSENT
    property int opened: XMLHttpRequest.OPENED
    property int headers_received: XMLHttpRequest.HEADERS_RECEIVED
    property int loading: XMLHttpRequest.LOADING
    property int done: XMLHttpRequest.DONE

    Component.onCompleted: {
        // Attempt to overwrite and delete values
        XMLHttpRequest.UNSENT = 9;
        XMLHttpRequest.OPENED = 9;
        XMLHttpRequest.HEADERS_RECEIVED = 9;
        XMLHttpRequest.LOADING = 9;
        XMLHttpRequest.DONE = 9;

        delete XMLHttpRequest.UNSENT;
        delete XMLHttpRequest.OPENED;
        delete XMLHttpRequest.HEADERS_RECEIVED;
        delete XMLHttpRequest.LOADING;
        delete XMLHttpRequest.DONE;
    }
}
