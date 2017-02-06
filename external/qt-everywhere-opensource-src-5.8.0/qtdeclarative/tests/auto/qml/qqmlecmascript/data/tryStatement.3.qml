import Qt.test 1.0

MyQmlObject {
    property int defaultValue: 123

    function go() {
        undefinedObject.method() // this call will throw an exception
        return 321
    }

    value: try { var p = go() } catch(e) { var p = defaultValue } finally { p == 123 }
}

