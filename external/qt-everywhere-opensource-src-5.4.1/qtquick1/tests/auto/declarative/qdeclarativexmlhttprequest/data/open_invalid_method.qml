import QtQuick 1.0

QtObject {
    property bool exceptionThrown: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        try {
            x.open("BLAH", "http://www.qt-project.org");
        } catch (e) {
            if (e.code == DOMException.SYNTAX_ERR)
                exceptionThrown = true;
        }
    }
}
