import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    // Enum property type
    enumProperty: MyQmlObject.EnumValue2

    // Enum property whose value is from a related type
    relatedEnumProperty: MyQmlObject.RelatedValue

    // Enum property whose value is defined in an unrelated type
    unrelatedEnumProperty: MyTypeObject.RelatedValue

    // Enum property whose value is defined in the Qt namespace
    qtEnumProperty: Qt.CaseInsensitive

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

    // Enums from a related type
    property int k: MyQmlObject.RelatedValue

    // Enum values defined both in a type and a related type
    property int l: MyQmlObject.MultiplyDefined
}
