import QtQuick 2.0
import Test 1.0

MyTypeObject {
    enumProperty: MyTypeObject.EnumVal1
    Component.onCompleted: {
        var a = MyTypeObject.EnumVal1;
        var b = MyTypeObject.lowercaseEnumVal
    }
}
