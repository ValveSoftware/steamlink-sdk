import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false

    qjsvalue: 6

    Component.onCompleted: {
        if (qjsvalue != 6) return;
        test = true;
    }
}
