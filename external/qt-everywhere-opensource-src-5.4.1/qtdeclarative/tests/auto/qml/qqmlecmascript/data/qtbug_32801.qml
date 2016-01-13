import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    signal testSignal(string arg)

    function emitTestSignal() {
        testSignal("test")
    }
}
