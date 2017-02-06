import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    property real base: 50
    property alias test1: myObject.test1
    property alias test2: myObject.test2

    objectProperty: QtObject {
        id: myObject
        property real test1: base
        property real test2: Math.max(0, base)
    }

    // Signal with no args
    onBasicSignal: base = 200
    // Signal with args
    onArgumentSignal: base = 400
}

