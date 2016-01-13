import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    property bool test: false

    id: root

    Component.onCompleted: root.argumentSignal.connect(root, methodNoArgs);
}

