import QtQuick 2.0

QtObject {
    property int readyState
    property bool statusIsException: false
    property bool statusTextIsException: false
    property string responseText
    property bool responseXMLIsNull

    Component.onCompleted: {
        var xhr = new XMLHttpRequest();

        readyState = xhr.readyState;
        try {
            status = xhr.status;
        } catch (error) {
            if (error.code == DOMException.INVALID_STATE_ERR)
                statusIsException = true;
        }
        try {
            statusText = xhr.statusText;
        } catch (error) {
            if (error.code == DOMException.INVALID_STATE_ERR)
                statusTextIsException = true;
        }
        responseText = xhr.responseText;
        responseXMLIsNull = (xhr.responseXML == null);
    }
}

