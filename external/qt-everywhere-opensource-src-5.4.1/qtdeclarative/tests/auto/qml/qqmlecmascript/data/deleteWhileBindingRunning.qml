import Qt.test 1.0

MyDeleteObject {
    property int result: nestedObject.intProperty + deleteNestedObject
}
