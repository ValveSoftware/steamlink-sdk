import QtQuick 2.0

QtObject {
    property int unsent
    property int opened
    property int headers_received
    property int loading
    property int done

    Component.onCompleted: {
        // Attempt to overwrite and delete values
        var x = new XMLHttpRequest();

        try { x.UNSENT = 9; } catch (e) {}
        try { x.OPENED = 9; } catch (e) {}
        try { x.HEADERS_RECEIVED = 9; } catch (e) {}
        try { x.LOADING = 9; } catch (e) {}
        try { x.DONE = 9; } catch (e) {}

        try { delete x.UNSENT; } catch (e) {}
        try { delete x.OPENED; } catch (e) {}
        try { delete x.HEADERS_RECEIVED; } catch (e) {}
        try { delete x.LOADING; } catch (e) {}
        try { delete x.DONE; } catch (e) {}

        unsent = x.UNSENT
        opened = x.OPENED
        headers_received = x.HEADERS_RECEIVED
        loading = x.LOADING
        done = x.DONE
    }
}

