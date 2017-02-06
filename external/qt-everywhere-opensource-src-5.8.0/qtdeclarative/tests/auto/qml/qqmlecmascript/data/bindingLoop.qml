import Qt.test 1.0

MyQmlContainer {
    children : [
        MyQmlObject {
            id: object1
            stringProperty: "hello" + object2.stringProperty
        },
        MyQmlObject {
            id: object2
            stringProperty: "hello" + object1.stringProperty
        }
    ]
}
