import QtQuick 2.0

QtObject {
    id: obj
    property string url
    property string which
    property bool threw: false

    onWhichChanged: {
        var x = new XMLHttpRequest;

        x.onreadystatechange = function() {
            if (x.readyState == which) {
                obj.threw = true
                throw(new Error("Exception from Callback"))
            }
        }

        x.open("GET", url);
        x.setRequestHeader("Test-header", "TestValue");
        x.send();
    }
}


