import QtQuick 2.0
import Qt.test 1.0

QtObject {
    property int qobjectPropertyTest: QObject.qobjectTestProperty
    property int qobjectMethodTest: 2

    Component.onCompleted: {
        qobjectMethodTest = QObject.qobjectTestMethod();
    }
}
