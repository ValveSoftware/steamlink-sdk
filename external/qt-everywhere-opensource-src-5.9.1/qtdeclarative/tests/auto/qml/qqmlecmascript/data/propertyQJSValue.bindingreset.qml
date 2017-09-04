import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    property bool test: false

    property bool doReset: false
    qjsvalueWithReset: if (doReset) return undefined; else return 9

    Component.onCompleted: {
        if (qjsvalueWithReset != 9) return;
        doReset = true

        if (qjsvalueWithReset != "Reset!") return;

        test = true;
    }
}
