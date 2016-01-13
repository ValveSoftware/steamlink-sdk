import QtQuick 2.0

QtObject {
    property string url

    property bool dataOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", url);
        x.setRequestHeader("Accept-Language","en-US");

        x.setRequestHeader("Test-header", "value");
        //Setting headers with just different cases
        //will be treated as the same header, and accepted
        //as the last setting.
        x.setRequestHeader("Test-hEADEr2", "value");
        x.setRequestHeader("Test-header2", "value2");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText == "QML Rocks!\n");
            }
        }

        x.send();
    }
}
