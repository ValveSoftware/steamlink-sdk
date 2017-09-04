import Qt.test 1.0

MyQmlObject {
    id: myObject
    onBasicSignal: myObject.methodNoArgs()
}
