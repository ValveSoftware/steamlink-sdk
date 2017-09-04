import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false

    qjsvalue: { 'color': 'red', 'width': 100 }
    property int bound: qjsvalue.width

    Component.onCompleted: {
        if (bound != 100) return;

        qjsvalue = { 'color': 'blue', 'width': 200 }      // bound should now be 200

        if (bound != 200) return;

        test = true;
    }
}
