import QtQuick 1.0

QtObject {
    property string reqType
    property string url

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;
        x.open(reqType, url);
        x.setRequestHeader("Accept-Language","en-US");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                if (reqType == "HEAD")
                    dataOK = (x.responseText == "");
                else
                    dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send("Data To Ignore");
    }
}

