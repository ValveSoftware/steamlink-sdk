import Test 1.0
MyContainer {
    property variant object : myObjectId

    MyTypeObject {
        id: "myObjectId"
    }

    MyTypeObject {
        selfGroupProperty.id: "name.with.dots"
    }
}
