import QtQuick 2.0

QtObject {
    property string url;
    property bool result: false
    property string correctjsondata : "{\"widget\":{\"debug\":\"on\",\"window\":{\"name\":\"main_window\",\"width\":500}}}"

    Component.onCompleted: {
        var request = new XMLHttpRequest();
        request.open("GET", url, true);
        request.responseType = "json";

        request.onreadystatechange = function() {
            if (request.readyState == XMLHttpRequest.DONE) {
                var jsonData = JSON.stringify(request.response);
                result = (correctjsondata == jsonData);
            }
        }

        request.send(null);
    }
}
