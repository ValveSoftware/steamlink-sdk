import QtQuick 1.0
import Test 1.0

QtObject {
    property QtObject myProperty
    property QtObject myProperty2
    property QtObject myProperty3
    property QtObject myProperty4
    property MyQmlObject myProperty5
    property MyQmlObject myProperty6

    myProperty: CompositeType {}
    myProperty2: CompositeType2 {}
    myProperty3: CompositeType3 {}
    myProperty4: CompositeType4 {}
    myProperty5: CompositeType2 {}
    myProperty6: CompositeType4 {}
}
