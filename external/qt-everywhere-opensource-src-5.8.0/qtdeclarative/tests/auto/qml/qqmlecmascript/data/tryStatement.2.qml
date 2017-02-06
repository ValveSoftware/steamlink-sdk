import Qt.test 1.0

MyQmlObject {
    property int defaultValue: 123

    function go() {
        return 321
    }

    value: try { go() } catch(e) { defaultValue }
}
