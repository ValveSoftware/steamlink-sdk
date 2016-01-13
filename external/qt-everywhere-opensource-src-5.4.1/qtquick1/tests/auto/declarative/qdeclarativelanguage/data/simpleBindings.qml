import Test 1.0
MyTypeObject {
    id: me
    property int v1: 10
    property int v2: 11

    property int value1
    property int value2
    property int value3
    property int value4

    value1: v1
    value2: me.v1
    value3: v1 + v2
    value4: Math.min(v1, v2)

    objectProperty: me
}
