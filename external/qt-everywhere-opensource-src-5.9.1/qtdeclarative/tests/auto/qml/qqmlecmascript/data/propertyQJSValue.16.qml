import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false
    property string string1

    qjsvalue: function () {
        string1 = "Test case 1"
        return 100;
    }

    Component.onCompleted: {
        if (qjsvalue() != 100)
            return

        if (string1 != "Test case 1")
            return;

        test = true;
    }
}
