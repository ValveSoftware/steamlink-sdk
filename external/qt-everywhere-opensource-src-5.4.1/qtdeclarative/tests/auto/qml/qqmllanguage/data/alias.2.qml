import Test 1.0

MyQmlObject {
    id: root
    property alias aliasObject: root.qmlobjectProperty

    qmlobjectProperty: MyQmlObject { value : 10 }
}
