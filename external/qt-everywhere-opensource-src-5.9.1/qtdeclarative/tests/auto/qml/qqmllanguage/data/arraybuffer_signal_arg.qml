import QtQuick 2.7
import Test 1.0

MyArrayBufferTestClass {
    property bool ok: false
    onByteArraySignal: {
        var view = new DataView(arg);
        ok = arg instanceof ArrayBuffer
            && arg.byteLength == 2
            && view.getUint8(0) == 42
            && view.getUint8(1) == 43;
    }
    Component.onCompleted: emitByteArraySignal(42, 2)
}
