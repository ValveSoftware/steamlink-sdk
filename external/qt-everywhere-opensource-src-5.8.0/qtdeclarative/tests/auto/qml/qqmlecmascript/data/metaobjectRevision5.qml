import Qt.test 1.1
import QtQuick 2.0

MyItemUsingRevisionedObject {
    property real test

    revisioned.prop1: 10
    revisioned.prop2: 1

    Component.onCompleted: test = revisioned.prop1 + revisioned.prop2
}

