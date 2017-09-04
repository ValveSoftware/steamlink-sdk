import Test 1.0
import QtQuick 2.0

MyQmlObject {
    property int a: Math.max(10, 9)
    property int b: 11
    Component.onCompleted: console.log("Completed " + a + " " + b);
}
