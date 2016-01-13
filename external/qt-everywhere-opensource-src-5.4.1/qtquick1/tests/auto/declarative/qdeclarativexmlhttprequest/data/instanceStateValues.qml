import QtQuick 1.0

QtObject {
    property int unsent
    property int opened
    property int headers_received
    property int loading
    property int done

    Component.onCompleted: {
        // Attempt to overwrite and delete values
        var x = new XMLHttpRequest();

        x.UNSENT = 9;
        x.OPENED = 9;
        x.HEADERS_RECEIVED = 9;
        x.LOADING = 9;
        x.DONE = 9;

        delete x.UNSENT;
        delete x.OPENED;
        delete x.HEADERS_RECEIVED;
        delete x.LOADING;
        delete x.DONE;

        unsent = x.UNSENT
        opened = x.OPENED
        headers_received = x.HEADERS_RECEIVED
        loading = x.LOADING
        done = x.DONE
    }
}

