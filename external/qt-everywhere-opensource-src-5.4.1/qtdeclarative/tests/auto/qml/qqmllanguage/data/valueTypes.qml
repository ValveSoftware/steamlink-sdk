import Test 1.0
MyTypeObject {
    rectProperty.x: 10
    rectProperty.y: 11
    rectProperty.width: rectProperty.x + 2
    rectProperty.height: 13

    intProperty: rectProperty.x

    onAction: { var a = rectProperty; a.x = 12; }

    rectProperty2: rectProperty
}
