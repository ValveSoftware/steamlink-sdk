import Qt.test 1.0

MyDeferredObject {
    id: root
    value: 10
    objectProperty: MyQmlObject {
        value: root.value
    }
    objectProperty2: MyQmlObject { id: blah }
}
