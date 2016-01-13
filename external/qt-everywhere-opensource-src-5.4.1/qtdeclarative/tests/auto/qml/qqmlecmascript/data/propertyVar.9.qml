import QtQuick 2.0
import Qt.test 1.0

Item {
    property bool test: false

    MyQmlObject {
        id: qmlobject
        intProperty: 5
    }
    property var qobjectVar: qmlobject
    property int bound: qobjectVar.intProperty

    Component.onCompleted: {
        if (bound != 5) return;

        test = true;
    }
}
