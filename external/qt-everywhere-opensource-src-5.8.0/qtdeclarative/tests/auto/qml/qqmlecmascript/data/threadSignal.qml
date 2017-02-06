import Qt.test 1.0
import QtQuick 2.0

MyWorkerObject {
    property bool passed: false
    Component.onCompleted: doIt()
    onDone: passed = (result == 'good')
}
