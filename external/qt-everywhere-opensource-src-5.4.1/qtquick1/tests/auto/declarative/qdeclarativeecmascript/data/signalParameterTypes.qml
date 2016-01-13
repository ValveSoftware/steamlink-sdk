import Qt.test 1.0

MyQmlObject
{
    id: root
    property int intProperty
    property real realProperty
    property color colorProperty
    property variant variantProperty
    property int enumProperty
    property int qtEnumProperty

    signal mySignal(int a, real b, color c, variant d, int e, int f)

    onMySignal: { intProperty = a; realProperty = b; colorProperty = c; variantProperty = d; enumProperty = e; qtEnumProperty = f; }

    onBasicSignal: root.mySignal(10, 19.2, Qt.rgba(1, 1, 0, 1), Qt.rgba(1, 0, 1, 1), MyQmlObject.EnumValue3, Qt.LeftButton)
}
