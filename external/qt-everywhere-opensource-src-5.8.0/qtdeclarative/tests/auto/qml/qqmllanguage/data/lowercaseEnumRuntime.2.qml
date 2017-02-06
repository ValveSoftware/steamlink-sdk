import QtQuick 2.0
import Test 1.0

MyTypeObject {
    intProperty: MyTypeObjectSingleton.EnumVal1
    Component.onCompleted: {
        var a = MyTypeObjectSingleton.EnumVal1;
        var b = MyTypeObjectSingleton.lowercaseEnumVal;
    }
}
