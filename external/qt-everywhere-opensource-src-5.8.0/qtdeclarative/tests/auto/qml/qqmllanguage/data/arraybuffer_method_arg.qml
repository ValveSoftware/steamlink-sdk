import QtQuick 2.7
import Test 1.0

MyArrayBufferTestClass {
    property bool ok: false
    Component.onCompleted: {
        var data = new Uint8Array([1, 2, 3]);
        var sum = byteArrayMethod_Sum(data.buffer);
        ok = sum == 6;
    }
}
