import Test 1.0
import QtQuick 2.0

MyTypeObject {
    // We set a and b to ensure that onCompleted is executed after bindings and
    // constants have been assigned
    property int a: Math.min(6, 7)
    Component.onDestruction: console.log("Destruction " + a + " " + nestedObject.b)

    objectProperty: OnDestructionType {
        qmlobjectProperty: MyQmlObject {
            id: nestedObject
            property int b: 10
            Component.onDestruction: console.log("Destruction " + a + " " + nestedObject.b)
        }
    }
}
