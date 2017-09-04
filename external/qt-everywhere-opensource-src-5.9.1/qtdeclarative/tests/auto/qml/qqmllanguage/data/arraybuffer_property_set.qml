import QtQuick 2.7
import Test 1.0

MyArrayBufferTestClass {
    property bool ok: false
    onByteArraySignal: ok = byteArrayProperty instanceof ArrayBuffer
    Component.onCompleted: {
        byteArrayProperty = new ArrayBuffer(42);
        ok = byteArrayProperty instanceof ArrayBuffer && byteArrayProperty.byteLength == 42;
    }
}
