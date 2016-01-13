import Qt.test 1.0
import QtQuick 1.0

MyQmlObject {
    objectProperty: otherObject

    property variant obj

    obj: QtObject {
        id: otherObject
    }
}
