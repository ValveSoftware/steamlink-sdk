import Qt.test 1.0
import QtQuick 2.0

MyDeferredObject {
    value: undefined // error is resolved before complete
    objectProperty: undefined // immediate error
    objectProperty2: QtObject {
        Component.onCompleted: value = 10
    }
}
