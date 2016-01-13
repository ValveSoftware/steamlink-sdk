import Qt.test 1.1
import QtQuick 1.0

QtObject {
    property variant a
    property real test

    a: MyRevisionedClass {
        prop2: 11

        Component.onCompleted: test = prop2
    }
}

