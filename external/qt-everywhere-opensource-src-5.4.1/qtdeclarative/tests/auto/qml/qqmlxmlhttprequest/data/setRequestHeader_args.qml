import QtQuick 2.0

QtObject {
    property bool exceptionThrown: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "testdocument.html");

        try {
            x.setRequestHeader("Test-header");
        } catch (e) {
            if (e.code == DOMException.SYNTAX_ERR)
                exceptionThrown = true;
        }
    }
}
