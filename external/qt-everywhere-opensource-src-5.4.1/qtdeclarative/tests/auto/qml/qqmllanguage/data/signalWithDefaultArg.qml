import Test 1.0
import QtQuick 2.0

MyQmlObject {
    property real signalCount: 0
    property real signalArg: 0

    signal noArgSignal
    signal argSignal(real arg)

    function emitNoArgSignal() { noArgSignal(); }
    function emitArgSignal() { argSignal(22); }

    onSignalWithDefaultArg: {
        signalArg = parameter
        signalCount++
    }

    Component.onCompleted: {
        noArgSignal.connect(signalWithDefaultArg)
        argSignal.connect(signalWithDefaultArg)
    }
}
