import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    objectProperty: MyQmlObject {}

    Component.onCompleted: {
        objectProperty = null;
    }
}

