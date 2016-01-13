import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    property bool test: false

    qjsvalue: new vehicle(4);
    property int wheelCount: qjsvalue.wheels

    function vehicle(wheels) {
        this.wheels = wheels;
    }

    Component.onCompleted: {
        qjsvalue.wheels = 6;  // not bindable, wheelCount shouldn't update

        if (qjsvalue.wheels != 6) return;
        if (wheelCount != 4) return;

        test = true;
    }
}
