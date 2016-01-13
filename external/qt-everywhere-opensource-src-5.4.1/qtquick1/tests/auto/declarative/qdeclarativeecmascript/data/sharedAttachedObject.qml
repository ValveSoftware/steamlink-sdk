import Qt.test 1.0
import QtQuick 1.0

MyQmlObject {
    id: root
    property bool test1: false
    property bool test2: false

    MyQmlObject.value2: 7

    Component.onCompleted: {
        test1 = root.MyQmlObject.value2 == 7;
        test2 = root.MyQmlObjectAlias.value2 == 7;
    }
}

