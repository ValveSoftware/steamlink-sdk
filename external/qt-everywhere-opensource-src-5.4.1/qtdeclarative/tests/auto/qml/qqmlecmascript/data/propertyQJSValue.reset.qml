import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root
    property bool test: false

    qjsvalueWithReset: 9

    Component.onCompleted: {
        qjsvalueWithReset = undefined

        if (qjsvalueWithReset != "Reset!") return;

        test = true;
    }
}
