import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    property bool test: false

    qjsvalue: new vehicle(8);
    property int wheelCount: qjsvalue.wheels

    function vehicle(wheels) {
        this.wheels = wheels;
    }

    Component.onCompleted: {
        if (wheelCount != 8) return;

        // not bindable, but wheelCount will update because truck itself changed.
        qjsvalue = new vehicle(12);

        if (wheelCount != 12) return;

        test = true;
    }
}
