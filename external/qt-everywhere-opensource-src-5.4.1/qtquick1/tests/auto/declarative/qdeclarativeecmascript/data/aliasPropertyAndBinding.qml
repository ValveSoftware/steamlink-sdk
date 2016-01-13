import QtQuick 1.0
import Qt.test 1.0

MyQmlObject {
    property alias c1: myObject.c1
    property int c2: 3
    property int c3: c2
    objectProperty: QtObject {
        id: myObject
        property int c1
    }
}


