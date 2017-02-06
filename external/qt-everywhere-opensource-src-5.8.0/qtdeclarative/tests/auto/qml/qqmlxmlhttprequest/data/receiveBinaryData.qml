import QtQuick 2.0

QtObject {
    property string url
    property int readSize: 0
    property int status: 0

    Component.onCompleted: {

        var request = new XMLHttpRequest();
        request.open("GET", url);
        request.responseType = "arraybuffer";

        request.onreadystatechange = function() {
            if (request.readyState == XMLHttpRequest.DONE) {
                status = request.status;
                var arrayBuffer = request.response;
                if (arrayBuffer) {
                    var byteArray = new Uint8Array(arrayBuffer);
                    readSize = byteArray.byteLength;
                }
            }
        }

        request.send(null);

    }
}

