import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false

    qjsvalue: { 'color': 'red', 'width': 100 }
    property int bound: qjsvalue.width

    Component.onCompleted: {
        if (bound != 100) return;

        qjsvalue.width = 200   // bound should remain 100

        if (bound != 100) return;

        test = true;
    }
}
