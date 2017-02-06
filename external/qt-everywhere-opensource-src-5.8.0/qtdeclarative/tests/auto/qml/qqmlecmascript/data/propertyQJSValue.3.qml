import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    property bool test: false

    qjsvalue: 4
    property int bound: qjsvalue + 5

    Component.onCompleted: {
        if (bound != 9) return;

        qjsvalue = qjsvalue + 1;

        if (bound != 10) return;

        test = true;
    }
}
