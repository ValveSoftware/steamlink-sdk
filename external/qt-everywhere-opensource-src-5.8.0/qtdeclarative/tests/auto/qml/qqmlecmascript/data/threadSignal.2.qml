import Qt.test 1.0
import QtQuick 2.0

MyWorkerObject {
    property bool passed: false
    onDone: passed = (result == 'good')
}
