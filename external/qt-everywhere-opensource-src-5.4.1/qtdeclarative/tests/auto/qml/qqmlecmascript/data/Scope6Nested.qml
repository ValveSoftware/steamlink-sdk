import Qt.test 1.0

MyQmlObject {
    function runtest(obj) {
        return obj.MyQmlObject.value == 19;
    }
}
