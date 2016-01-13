import Qt.test 1.0
import QtQuick 1.0

MyQmlObject {
    property bool test: false

    id: root

    function testFunction() {
        test = true;
    }

    Component.onCompleted: root.argumentSignal.connect(testFunction);
}

