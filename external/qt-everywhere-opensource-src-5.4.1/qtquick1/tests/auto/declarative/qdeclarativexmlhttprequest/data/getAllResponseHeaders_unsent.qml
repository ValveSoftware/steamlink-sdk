import QtQuick 1.0

QtObject {
    property bool test: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        try {
            x.getAllResponseHeaders();
        } catch (e) {
            if (e.code == DOMException.INVALID_STATE_ERR)
                test = true;
        }
    }
}
