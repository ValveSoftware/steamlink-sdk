import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool test: false

    property MyQmlObject obj: MyQmlObject {
        id: qmlobject
        intProperty: 5
    }
    qjsvalue: qmlobject
    property int bound: qjsvalue.intProperty

    Component.onCompleted: {
        if (bound != 5) return;

        test = true;
    }
}
