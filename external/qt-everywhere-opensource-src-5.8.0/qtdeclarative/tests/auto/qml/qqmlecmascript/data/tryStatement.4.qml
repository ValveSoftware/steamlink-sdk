import Qt.test 1.0

MyQmlObject {
    property int defaultValue: 123

    function go() {
        return 321
    }

    value: try { var p = go() } catch(e) { var p = defaultValue } finally { p == 321 }
}

