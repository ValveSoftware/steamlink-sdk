import Qt.test 1.0
import QtQml 2.0

MyDeferredObject {
    id: root
    property QtObject target: null
    objectProperty: MyQmlObject {
        value: target.value
    }
}
