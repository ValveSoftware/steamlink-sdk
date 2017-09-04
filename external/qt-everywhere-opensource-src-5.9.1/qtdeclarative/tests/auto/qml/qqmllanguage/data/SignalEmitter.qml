import QtQml 2.0

QtObject {
    // these two need to be set by the test qml
    property QtObject testObject
    property bool handleSignal

    property SignalParam p: SignalParam { }
    property OtherSignalParam op: OtherSignalParam { }
    signal testSignal(SignalParam spp);

    function emitTestSignal() {
        testObject.expectNull = true;
        testSignal(op);

        testObject.expectNull = false;
        testSignal(p);
    }

    onTestSignal: {
        if (handleSignal == true) {
            testObject.determineSuccess(spp);
        }
    }
}
