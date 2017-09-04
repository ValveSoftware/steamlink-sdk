import Test 1.0

MyTypeObject {
    property variant object
    object: MyTypeObject {
        rect.x: 19
        rect.y: 33
        rect.width: 5
        rect.height: 99
    }

    rect: object.rect
}
