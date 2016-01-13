import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: root

    property int foo: 12

    property bool test1: foo == 12
    property bool test2: console != 11
    property bool test3: root.console == 11
}

