import QtQuick 2.7
import Test 1.0

MyArrayBufferTestClass {
    property bool ok: false
    Component.onCompleted: {
        var buf = byteArrayMethod_CountUp(1, 3);
        var view = new DataView(buf);
        ok = buf instanceof ArrayBuffer
           && buf.byteLength == 3
           && view.getUint8(0) == 1
           && view.getUint8(1) == 2
           && view.getUint8(2) == 3;
    }
}
