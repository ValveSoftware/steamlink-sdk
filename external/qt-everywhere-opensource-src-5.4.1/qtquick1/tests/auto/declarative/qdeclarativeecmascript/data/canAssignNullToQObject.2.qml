import Qt.test 1.0
import QtQuick 1.0

MyQmlObject {
    objectProperty: MyQmlObject {}

    Component.onCompleted: {
        objectProperty = null;
    }
}

