import Qt.test 1.0
import QtQuick 2.0
import "scriptConnect.6.js" as Script

MyQmlObject {
    property int test: 0

    id: root

    Component.onCompleted: {
        root.argumentSignal.connect(Script.testFunction);
        root.argumentSignal.connect(Script.testFunction);
    }
}

