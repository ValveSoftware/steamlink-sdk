import QtQuick 2.0
import Test 1.0

QtObject {
    property QtObject myProperty
    property QtObject myProperty2
    property QtObject myProperty3
    property QtObject myProperty4
    property MyQmlObject myProperty5
    property MyQmlObject myProperty6
    property CompositeType myProperty7
    property CompositeType myProperty8
    property CompositeType2 myProperty9
    property CompositeType2 myPropertyA

    myProperty: CompositeType {}
    myProperty2: CompositeType2 {}
    myProperty3: CompositeType3 {}
    myProperty4: CompositeType4 {}
    myProperty5: CompositeType2 {}
    myProperty6: CompositeType4 {}
    myProperty7: CompositeType {}
    myProperty8: CompositeType5 {}
    myProperty9: CompositeType2 {}
    myPropertyA: CompositeType6 {}
}
