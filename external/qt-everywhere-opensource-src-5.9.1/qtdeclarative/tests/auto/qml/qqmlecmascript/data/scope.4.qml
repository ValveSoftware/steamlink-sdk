import Qt.test 1.0

MyQmlObject {
    id: a
    property int b: 9

    property int test
    property string test2

    // Should resolve to signal arguments, not to other elements in the file
    onArgumentSignal: { test = a; test2 = b; }
}
