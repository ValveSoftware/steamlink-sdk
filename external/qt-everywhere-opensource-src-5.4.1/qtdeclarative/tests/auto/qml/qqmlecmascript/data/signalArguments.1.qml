import Qt.test 1.0

MyQmlObject {
    property int argumentCount: -1
    onBasicSignal: {
        argumentCount = arguments.length
        setString('pass')
    }
}
