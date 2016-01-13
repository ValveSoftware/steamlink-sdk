import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false

    qjsvalue: [1, 2, 3, "four", "five"]
    property int bound: qjsvalue[0]

    Component.onCompleted: {
        if (bound != 1) return;

        qjsvalue[0] = 10      // bound should remain 1

        if (bound != 1) return;

        test = true;
    }
}
