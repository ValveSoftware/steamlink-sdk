import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    // Enums from non-namespaced type
    property int a: MyQmlObject.EnumValue1
    property int b: MyQmlObject.EnumValue2
    property int c: MyQmlObject.EnumValue3
    property int d: MyQmlObject.EnumValue4

    // Enums from namespaced type
    property int e: Namespace.MyQmlObject.EnumValue1
    property int f: Namespace.MyQmlObject.EnumValue2
    property int g: Namespace.MyQmlObject.EnumValue3
    property int h: Namespace.MyQmlObject.EnumValue4

    // Test that enums don't mask attached properties
    property int i: MyQmlObject.value
    property int j: Namespace.MyQmlObject.value
}
