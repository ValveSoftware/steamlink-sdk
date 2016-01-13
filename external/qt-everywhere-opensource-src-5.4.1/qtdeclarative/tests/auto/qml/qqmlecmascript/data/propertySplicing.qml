import Qt.test 1.0
import QtQuick 2.0

MyDerivedObject {
    property bool test: false

    Component.onCompleted: {
        test = intProperty()
    }
}
