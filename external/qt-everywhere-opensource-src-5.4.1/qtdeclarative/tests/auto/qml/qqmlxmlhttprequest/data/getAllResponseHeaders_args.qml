import QtQuick 2.0

QtObject {
    property bool exceptionThrown: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "testdocument.html");
        x.send();

        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {
                try {
                    x.getAllResponseHeaders("Test-header");
                } catch (e) {
                    if (e.code == DOMException.SYNTAX_ERR)
                        exceptionThrown = true;
                }
            }
        }
    }
}
